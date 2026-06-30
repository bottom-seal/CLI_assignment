#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Options {
    std::string api;
    std::string token;
    std::map<std::string, std::string> params;
};

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

//convert s to 's', handles single quotes in s by escaping them
static std::string quote(const std::string& s) {
    std::string out = "'";

    //if a string contains a ', need extra handling (I dont think this case happens)
    for (char ch : s) {
        out += ch == '\'' ? "'\\''" : std::string(1, ch);
    }

    return out + "'";
}

static std::string command_output(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r");

    if (!pipe) {
        throw std::runtime_error("failed to run graphmeta");
    }

    std::string out;
    char buffer[4096];

    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        out += buffer;
    }

    if (pclose(pipe) != 0) {
        throw std::runtime_error("graphmeta failed");
    }

    return out;
}

static std::string url_encode(CURL* curl, const std::string& value) {
    //char *curl_easy_escape(CURL *curl, const char *string, int length);
    char* raw = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));//encode the value, allocates new string

    if (!raw) {
        throw std::runtime_error("URL encoding failed");
    }

    std::string out(raw);
    curl_free(raw);
    return out;
}
//split string at first delimiter (eg. val=1)
static bool split_once(const std::string& s, char delimiter, std::string& a, std::string& b) {
    size_t pos = s.find(delimiter);

    if (pos == std::string::npos || pos == 0) {
        return false;
    }

    a = s.substr(0, pos);
    b = s.substr(pos + 1);
    return true;
}

//./graphsend <api> [--param name=value] [--token token]
static Options parse(int argc, char** argv) {
    if (argc < 2) {
        throw std::runtime_error("missing api name");
    }

    Options options;
    options.api = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--token" && i + 1 < argc) {
            options.token = argv[++i];
        } else if (arg == "--param" && i + 1 < argc) {
            std::string key;
            std::string value;

            if (!split_once(argv[++i], '=', key, value)) {
                throw std::runtime_error("--param expects name=value");
            }

            options.params[key] = value;
        } else {
            throw std::runtime_error("unknown or incomplete option: " + arg);
        }
    }

    if (options.token.empty()) {
        const char* env = std::getenv("GRAPH_TOKEN");
        options.token = env ? env : "";
    }

    return options;
}
//take output of graphmeta
static std::string full_url(const json& meta) {
    std::string base = meta.value("baseUrl", "");
    std::string path = meta.value("path", "");

    if (base.empty()) {
        throw std::runtime_error("metadata has no baseUrl");
    }

    return path.empty() ? base : base + path;
}
//$smth in shell might be treated as shell variable, user don't need to typee $, convert with this function for OData convention
static std::string param_name(const json& meta, const std::string& name) {
    if (!starts_with(name, "$")) {
        std::string odata = "$" + name;

        for (const auto& param : meta.value("params", json::array())) {
            if (param.value("name", "") == odata) {
                return odata;
            }
        }
    }

    return name;
}

static json build_request(CURL* curl, const json& meta, const Options& options) {
    std::string url = full_url(meta);
    json query = json::object();//query param holder
    json headers = json::array({"Accept: application/json"});//array for headers

    if (!options.token.empty()) {
        headers.push_back("Authorization: Bearer " + options.token);//if has token, add Authorization header
    }

    char separator = '?';//first query param starts with ?, subsequent ones with &

    for (const auto& [raw_name, value] : options.params) {
        std::string name = param_name(meta, raw_name);//prefix $ if needed
        query[name] = value;
        url += separator;
        url += url_encode(curl, name) + "=" + url_encode(curl, value);
        separator = '&';
    }

    return {
        {"apiName", meta.value("apiName", options.api)},//if exist, use apiName, else use options.api
        {"method", meta.value("method", "GET")},
        {"url", url},
        {"headers", headers},
        {"query", query}
    };
}

int main(int argc, char** argv) {
    try {
        Options options = parse(argc, argv);
        json meta = json::parse(command_output("./graphmeta " + quote(options.api)));

        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL* curl = curl_easy_init();

        if (!curl) {
            throw std::runtime_error("failed to initialize curl");
        }

        std::cout << build_request(curl, meta, options).dump(4) << "\n";
        curl_easy_cleanup(curl);
        curl_global_cleanup();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Usage: graphsend <api> [--param name=value] [--token token]\n";
        return 1;
    }
}
