#include <fstream>
#include <random>
#include <iostream>
#include <curl/curl.h>
#include <string>

std::string getOrCreateMachineId() {
    std::string idFile = "machine_id.txt";
    std::ifstream infile(idFile);
    std::string machineId;
    
    if (infile.is_open()) {
        std::getline(infile, machineId);
        infile.close();
    }
    
    if (machineId.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        machineId = "pc_" + std::to_string(dis(gen));
        
        std::ofstream outfile(idFile);
        if (outfile.is_open()) {
            outfile << machineId;
            outfile.close();
        }
    }
    return machineId;
}

// 1. Version complète à 4 arguments en premier
void sendGameStats(int score, int level, int playTimeSeconds, const std::string& name) {
    std::string machineId = getOrCreateMachineId();
    std::string fullDisplayName = name + " (" + machineId + ")";
    
    std::string jsonPayload = "{\n";
    jsonPayload += "  \"name\": \"" + fullDisplayName + "\",\n";
    jsonPayload += "  \"machine_id\": \"" + machineId + "\",\n";
    jsonPayload += "  \"score\": " + std::to_string(score) + ",\n";
    jsonPayload += "  \"level\": " + std::to_string(level) + ",\n";
    jsonPayload += "  \"play_time\": " + std::to_string(playTimeSeconds) + "\n";
    jsonPayload += "}";

    CURL *curl = curl_easy_init();
    if (!curl) return;

    std::string url = "http://petitain.be/scoredb.php";
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);

    curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// 2. Version à 3 arguments en second (elle connaît maintenant la version à 4 arguments)
void sendGameStats(int score, int level, const std::string& name) {
    sendGameStats(score, level, 0, name);
}