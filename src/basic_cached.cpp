#include <curl/curl.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

struct Api {
    const char* name;
    const char* path;
};

struct Remote {
    std::string sha;
    std::string download_url;
};

static const char* OPENAPI_FILE = "data/openapi.yaml";
static const char* CACHE_FILE = "data/api_cache.json";
static const char* GITHUB_INFO =
    "https://api.github.com/repos/microsoftgraph/msgraph-metadata/"
    "contents/openapi/v1.0/openapi.yaml?ref=master";

static const Api apis[] = {
    {"users", "/users"},
    {"service-principals", "/servicePrincipals"},
    {"conditional-access-policies", "/identity/conditionalAccess/policies"},
};

static std::string scalar(const YAML::Node& node) {
    if (!node || !node.IsScalar()) {
        return "";
    }

    try {
        return node.as<std::string>();
    } catch (const YAML::Exception&) {
        return "";
    }
}

static bool boolean(const YAML::Node& node) {
    if (!node || !node.IsScalar()) {
        return false;
    }

    try {
        return node.as<bool>();
    } catch (const YAML::Exception&) {
        return false;
    }
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

static std::string json_pointer_token(std::string s) {
    for (size_t pos = 0; (pos = s.find("~1", pos)) != std::string::npos; ++pos) {
        s.replace(pos, 2, "/");
    }

    for (size_t pos = 0; (pos = s.find("~0", pos)) != std::string::npos; ++pos) {
        s.replace(pos, 2, "~");
    }

    return s;
}

static YAML::Node pointer(const YAML::Node& root, const std::string& path) {
    if (!starts_with(path, "#/")) {
        return YAML::Node();
    }

    YAML::Node node = YAML::Clone(root);
    std::stringstream parts(path.substr(2));
    std::string part;

    while (std::getline(parts, part, '/') && node) {
        node = node[json_pointer_token(part)];
    }

    return node;
}

static YAML::Node ref(const YAML::Node& root, YAML::Node node) {
    for (int i = 0; i < 16 && node && node["$ref"]; ++i) {
        YAML::Node resolved = pointer(root, scalar(node["$ref"]));

        if (!resolved) {
            break;
        }

        node = resolved;
    }

    return node;
}

static std::string type_of(const YAML::Node& root, YAML::Node schema) {
    schema = ref(root, schema);
    std::string type = scalar(schema["type"]);

    if (type == "array") {
        return "array<" + type_of(root, schema["items"]) + ">";
    }

    if (!type.empty()) {
        return type;
    }

    for (const char* key : {"allOf", "anyOf", "oneOf"}) {
        YAML::Node options = schema[key];

        if (options && options.IsSequence() && options.size() > 0) {
            return type_of(root, options[0]);
        }
    }

    return schema["properties"] ? "object" : "unknown";
}

static std::string base_url(const YAML::Node& root) {
    YAML::Node servers = root["servers"];
    YAML::Node server = servers && servers.IsSequence() && servers.size() ? servers[0] : YAML::Node();
    std::string url = scalar(server["url"]);
    std::string version;

    if (server["variables"] && server["variables"]["version"]) {
        version = scalar(server["variables"]["version"]["default"]);
    }

    size_t pos = url.find("{version}");

    if (!version.empty() && pos != std::string::npos) {
        url.replace(pos, 9, version);
    }

    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }

    return url.empty() ? "https://graph.microsoft.com/v1.0" : url;
}

static const Api* find_api(const std::string& name) {
    for (const Api& api : apis) {
        if (name == api.name) {
            return &api;
        }
    }

    return nullptr;
}

static json parameters(const YAML::Node& root, YAML::Node operation) {
    json out = json::array();
    YAML::Node params = operation["parameters"];

    if (!params || !params.IsSequence()) {
        return out;
    }

    for (YAML::Node param : params) {
        param = ref(root, param);

        if (scalar(param["name"]).empty()) {
            continue;
        }

        json item = {
            {"name", scalar(param["name"])},
            {"in", scalar(param["in"])},
            {"required", boolean(param["required"])},
            {"type", type_of(root, param["schema"])}
        };
        std::string description = scalar(param["description"]);

        if (!description.empty()) {
            item["description"] = description;
        }

        out.push_back(item);
    }

    return out;
}

static json api_json(const YAML::Node& root, const Api& api) {
    YAML::Node operation = root["paths"][api.path]["get"];

    if (!operation) {
        throw std::runtime_error(std::string("GET operation not found for ") + api.path);
    }

    return {
        {"apiName", api.name},
        {"baseUrl", base_url(root)},
        {"method", "GET"},
        {"path", api.path},
        {"params", parameters(root, operation)}
    };
}

static size_t write_string(void* data, size_t size, size_t count, void* out) {
    static_cast<std::string*>(out)->append(static_cast<char*>(data), size * count);
    return size * count;
}

static bool http_get(const std::string& url, std::string& body) {
    CURL* curl = curl_easy_init();

    if (!curl) {
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");

    body.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "graphmeta-cache/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    CURLcode result = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result == CURLE_OK && code >= 200 && code < 300;
}

static bool remote_info(Remote& remote) {
    std::string body;

    if (!http_get(GITHUB_INFO, body)) {
        return false;
    }

    json info = json::parse(body, nullptr, false);

    if (!info.is_object()) {
        return false;
    }

    remote.sha = info.value("sha", "");
    remote.download_url = info.value("download_url", "");
    return !remote.sha.empty() && !remote.download_url.empty();
}

static json load_cache() {
    std::ifstream file(CACHE_FILE);
    json cache = json::object();

    try {
        if (file) {
            file >> cache;
        }
    } catch (const json::exception&) {
        cache = json::object();
    }

    return cache.is_object() ? cache : json::object();
}

static std::string cached_sha(const json& cache) {
    return cache.contains("_meta") && cache["_meta"].is_object()
        ? cache["_meta"].value("remote_sha", "")
        : "";
}

static void save_cache(const json& cache) {
    fs::create_directories(fs::path(CACHE_FILE).parent_path());
    std::ofstream(CACHE_FILE) << cache.dump(4) << "\n";
}

static bool download_yaml(const Remote& remote) {
    std::string yaml;

    if (!http_get(remote.download_url, yaml)) {
        return false;
    }

    fs::create_directories(fs::path(OPENAPI_FILE).parent_path());
    std::ofstream out(OPENAPI_FILE, std::ios::binary);
    out << yaml;
    return out.good();
}

static json build_cache(json cache, const Remote* remote, const Api* only = nullptr) {
    YAML::Node root = YAML::LoadFile(OPENAPI_FILE);

    if (!cache.is_object()) {
        cache = json::object();
    }

    if (remote) {
        cache["_meta"] = {
            {"source", GITHUB_INFO},
            {"remote_sha", remote->sha},
            {"openapi_file", OPENAPI_FILE}
        };
    } else if (!cache.contains("_meta")) {
        cache["_meta"] = {
            {"source", GITHUB_INFO},
            {"remote_sha", "unknown"},
            {"openapi_file", OPENAPI_FILE}
        };
    }

    if (only) {
        cache[only->name] = api_json(root, *only);
    } else {
        for (const Api& api : apis) {
            cache[api.name] = api_json(root, api);
        }
    }

    save_cache(cache);
    return cache;
}

static void usage() {
    std::cout << "Usage: graphmeta <api>|list|update|clear-cache\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "list") {
        for (const Api& api : apis) {
            std::cout << api.name << "\n";
        }

        return 0;
    }

    if (command == "clear-cache") {
        save_cache(json::object());
        return 0;
    }

    const Api* api = find_api(command);

    if (!api && command != "update") {
        std::cerr << "Unknown API\n";
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    try {
        json cache = load_cache();
        Remote remote;
        bool online = remote_info(remote);
        bool fresh = online && cached_sha(cache) == remote.sha;

        if (!online) {
            std::cerr << "Warning: GitHub metadata check failed; using cache or local YAML.\n";
        }

        if (command != "update" && fresh && cache.contains(api->name)) {
            std::cout << cache[api->name].dump(4) << "\n";
            curl_global_cleanup();
            return 0;
        }

        if (online && (!fresh || command == "update" || !fs::exists(OPENAPI_FILE))) {
            if (!download_yaml(remote)) {
                throw std::runtime_error("failed to download OpenAPI YAML");
            }

            if (!fresh || command == "update") {
                cache = json::object();
            }

            cache = build_cache(cache, &remote, command == "update" ? nullptr : api);
        } else if (cache.contains(command)) {
            std::cerr << "Warning: using cached metadata without a fresh GitHub check.\n";
            if (!cache.contains("_meta")) {
                cache["_meta"] = {
                    {"source", GITHUB_INFO},
                    {"remote_sha", "unknown"},
                    {"openapi_file", OPENAPI_FILE}
                };
                save_cache(cache);
            }
        } else {
            if (!fs::exists(OPENAPI_FILE)) {
                throw std::runtime_error(
                    "no cache entry found and data/openapi.yaml is missing"
                );
            }

            cache = build_cache(cache, online ? &remote : nullptr, api);
        }

        if (command == "update") {
            std::cout << "Cache updated.\n";
        } else {
            std::cout << cache[api->name].dump(4) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
