#include <execution>
#include <vector>
#include "document.h"
#include "process_queries.h"



std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> results(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), results.begin(),
        [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); });
    return results;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> results = ProcessQueries(search_server, queries);
    std::vector<Document> documents;
    documents.reserve(std::accumulate(results.begin(), results.end(), 0,
        [](int sum, const std::vector<Document>& docs) { return sum + docs.size(); }));
    for (const auto& result : results) {
        documents.insert(documents.end(), result.begin(), result.end());
    }
    return documents;
}