#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

using json = nlohmann::json;

struct Api {
    const char* name;
    const char* path;
};

static const Api apis[] = {
    {"users", "/users"},
    {"service-principals", "/servicePrincipals"},
    {"conditional-access-policies", "/identity/conditionalAccess/policies"},
};

//return string if node is scalar
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

//return bool if node is scalar
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

//prefix is a string that the string s starts with
static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}
//replace ~1 with / and ~0 with ~ in a string
static std::string json_pointer_token(std::string s) {
    for (size_t pos = 0; (pos = s.find("~1", pos)) != std::string::npos; ++pos) {
        s.replace(pos, 2, "/");
    }

    for (size_t pos = 0; (pos = s.find("~0", pos)) != std::string::npos; ++pos) {
        s.replace(pos, 2, "~");
    }

    return s;
}

//resolve a ref, return the node
static YAML::Node pointer(const YAML::Node& root, const std::string& path) {
    if (!starts_with(path, "#/")) {//from root, same document only
        return YAML::Node();
    }

    YAML::Node node = YAML::Clone(root);
    std::stringstream parts(path.substr(2));// skip "#/"
    std::string part;

    while (std::getline(parts, part, '/') && node) {
        node = node[json_pointer_token(part)];
    }

    return node;
}

//argument node should be a $ref node, return resolved node
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
    //check if schema has allOf, anyOf, oneOf and return type of first element
    for (const char* key : {"allOf", "anyOf", "oneOf"}) {
        YAML::Node options = schema[key];

        if (options && options.IsSequence() && options.size() > 0) {
            return type_of(root, options[0]);
        }
    }

    return schema["properties"] ? "object" : "unknown";
}

static std::string base_url(const YAML::Node& root) {
    YAML::Node server = root["servers"] && root["servers"].IsSequence() && root["servers"].size()
        ? root["servers"][0]
        : YAML::Node();
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
//return pointer to Api struct if name matches, otherwise return nullptr
static const Api* find_api(const std::string& name) {
    for (const Api& api : apis) {
        if (name == api.name) {
            return &api;
        }
    }

    return nullptr;
}
//extracts parameters from operation node and returns them as a json array
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
            item["description"] = description;//this line creates new field if not exist
        }

        out.push_back(item);
    }

    return out;
}

static void usage() {
    std::cout << "Usage: graphmeta <api>|list\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        usage();
        return 1;
    }

    std::string name = argv[1];

    if (name == "list") {
        for (const Api& api : apis) {
            std::cout << api.name << "\n";
        }

        return 0;
    }

    const Api* api = find_api(name);

    if (!api) {
        std::cerr << "Unknown API\n";
        return 1;
    }

    YAML::Node root = YAML::LoadFile("data/openapi.yaml");
    std::string path = api->path;
    YAML::Node operation = root["paths"][path]["get"];

    if (!operation) {
        std::cerr << "GET operation not found for " << path << "\n";
        return 1;
    }

    std::cout << json{
        {"apiName", api->name},
        {"baseUrl", base_url(root)},
        {"method", "GET"},
        {"path", path},
        {"params", parameters(root, operation)}
    }.dump(4) << "\n";
}
