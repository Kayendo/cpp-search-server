#pragma once

// добрый вечер, в ваших замечаниях было:
// using namespace в хедере (испр. в строке 23)
// Очень большой возвращаемый параметр, я бы сделал using (испр. в строке 55
// использован для всех функций MatchResult)
// Сюда стоит добавить string view в ккоторой хранить текст документа (испр. в строке 81)
// Не совсем понимаю, какие еще замечания нужно исправить, прошу указать

#include "string_processing.h"
#include "log_duration.h"
#include "document.h"
#include "concurrent_map.h"
#include <stdexcept>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <execution>
#include <string_view>
#include <thread>

// здесь было using namespace

const double COMPARE_TOLERANCE = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(std::string_view stop_words_text);
    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, 
                                 DocumentStatus status = DocumentStatus::ACTUAL) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> 
    FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, 
                     DocumentStatus status = DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    using MatchResult = std::tuple<std::vector<std::string_view>, DocumentStatus>; // using для возвращаемого параметра
    
   MatchResult MatchDocument(std::execution::parallel_policy policy, std::string_view raw_query, int document_id) const;

    MatchResult 
    MatchDocument(std::execution::sequenced_policy policy, std::string_view raw_query, int document_id) const;

    MatchResult 
    MatchDocument(std::string_view raw_query, int document_id) const;
    
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

    void RemoveDocument(int document_id){
        RemoveDocument(std::execution::seq, document_id);
    }

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string_view doc_text; // string view для хранения текста
    };
    
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<std::string> all_words_;
    std::map<int, DocumentData> documents_;
    std::set<int> id_list_;

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;
    [[nodiscard]] static bool IsValidWord(const std::string_view word);

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;

    SearchServer::Query ParseQuery(std::string_view text) const;
    SearchServer::Query ParseQueryForSeq(std::string_view text) const;

    
        template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query,
                                            DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query,
                                            DocumentPredicate document_predicate) const;
    
    double ComputeWordInverseDocumentFreq(std::string_view word) const;
};

// реализация шаблонов
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        :stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
            
    for (const auto& word : stop_words_){
        if(!IsValidWord(word))
            throw std::invalid_argument("Invalid word: " + word);
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
                                                         DocumentPredicate document_predicate) const{
    
    const auto query = ParseQueryForSeq(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < COMPARE_TOLERANCE) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
            });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return  matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                        DocumentPredicate document_predicate) const{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> 
SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const{

    return FindTopDocuments(policy,
            raw_query, 
            [status](int document_id, DocumentStatus document_status, int rating) {
                  return document_status == status;
            });
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query,
                                                     DocumentPredicate document_predicate) const{

    std::map<int, double> document_to_relevance;

    for (std::string_view word : query.plus_words) {
        auto it = word_to_document_freqs_.find(word);
        if(it == word_to_document_freqs_.end())
            continue;
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : it->second) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        auto it = word_to_document_freqs_.find(word);
        if(it == word_to_document_freqs_.end())
            continue;
        for (const auto [document_id, _] : it->second) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query,
                                                     DocumentPredicate document_predicate) const{

    ConcurrentMap<int, double> document_to_relevance(std::thread::hardware_concurrency());

    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), 
        [this, document_predicate, &document_to_relevance](std::string_view word) {
            auto it = word_to_document_freqs_.find(word);
            if(it == word_to_document_freqs_.end())
                return;
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : it->second) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }   
    });

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), 
        [this, &document_to_relevance](std::string_view word) {
            auto it = word_to_document_freqs_.find(word);
            if(it == word_to_document_freqs_.end())
                return;
            for (const auto [document_id, _] : it->second) {
                document_to_relevance.Erase(document_id);
            }
            
        });

    auto ordinary_map = document_to_relevance.BuildOrdinaryMap();

    std::vector<Document> matched_documents(ordinary_map.size());

    std::transform(
        std::execution::par,
        ordinary_map.begin(),
        ordinary_map.end(),
        matched_documents.begin(),
        [this](auto& id_relevance){
            return Document{id_relevance.first, id_relevance.second, documents_.at(id_relevance.first).rating};
    });

    return matched_documents;
}
template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id){
    id_list_.erase(document_id);   

    std::vector<std::string_view> words(document_to_word_freqs_[document_id].size());

    std::transform(policy,
        document_to_word_freqs_[document_id].begin(),
        document_to_word_freqs_[document_id].end(),
        words.begin(),
        [](const auto &word){
            return (word.first);
        }
    );

    std::for_each(policy, words.begin(), words.end(),
        [this, document_id](std::string_view word) {
            word_to_document_freqs_[word].erase(document_id);
        }
    );

    document_to_word_freqs_.erase(document_id);

    documents_.erase(document_id);   
}