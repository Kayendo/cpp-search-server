#include "search_server.h"

using namespace std;
using namespace std::string_literals;

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(
        SplitIntoWords(stop_words_text)){
}

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(
        SplitIntoWords(stop_words_text)){
}

void SearchServer::AddDocument(int document_id, string_view document,
		DocumentStatus status, const vector<int> &ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Negative ID"s);
    }
    if (documents_.find(document_id) != documents_.end()) {
        throw std::invalid_argument("ID exists"s);
    }
    if (!IsValidWord(document)) {
        throw std::invalid_argument("Text is invalid"s);
    }

    auto words = SplitIntoWordsNoStop(document);
    const std::string_view document_text{ document };
    documents_.emplace(document_id, DocumentData{ SearchServer::ComputeAverageRating(ratings), status, document_text });
    
    words = SplitIntoWordsNoStop(documents_.at(document_id).doc_text);

	const double inv_word_count = 1.0 / words.size();
	for (string_view word : words) {
		all_words_.insert(static_cast<string>(word));
		word_to_document_freqs_[*all_words_.find(static_cast<string>(word))][document_id] +=
				inv_word_count;
		document_to_word_freqs_[document_id][*all_words_.find(static_cast<string>(word))] +=
				inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ SearchServer::ComputeAverageRating(ratings), status, document_text });
	id_list_.insert(document_id);
}

using MatchResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;

MatchResult SearchServer::MatchDocument(string_view raw_query,
                                                                                      int document_id) const {
    return MatchDocument(execution::seq,raw_query,document_id);
}
 
MatchResult SearchServer::MatchDocument(const execution::sequenced_policy,
                                                                                      string_view raw_query,
                                                                                      int document_id) const {
    if (documents_.count(document_id) == 0) {
        throw out_of_range("Invalid id");
    }
 
    if (raw_query.empty()) {
        throw invalid_argument("Invalid query");
    }
 
    const auto result = ParseQueryForSeq(raw_query);
    vector<string_view> matched_words;
    for (auto word : result.minus_words) {
 
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {vector<basic_string_view<char>>{}, documents_.at(document_id).status};
 
        }
    }
    for (auto word : result.plus_words) {
 
        if (word_to_document_freqs_.count(word) == 0) {
 
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
 
    }
    return {matched_words, documents_.at(document_id).status};
}
 
MatchResult SearchServer::MatchDocument(const execution::parallel_policy,
                                                                                      string_view raw_query,
                                                                                      int document_id) const {
    if (documents_.count(document_id) == 0) {
        throw out_of_range("Invalid id");
    }

    if (raw_query.empty()) {
        throw invalid_argument("Invalid query");
    }
    const auto& result = ParseQuery(raw_query);
 
    const auto& check = [this, document_id](string_view word) {
        const auto help = word_to_document_freqs_.find(word);
        return help!= word_to_document_freqs_.end() &&
               help->second.count(document_id);
    };
 
    if (any_of(execution::par,
                    result.minus_words.begin(),
                    result.minus_words.end(),
                    check)) {
        return {vector<basic_string_view<char>>{}, documents_.at(document_id).status};
    }
 
    vector<string_view> matched_words(result.plus_words.size());
 
    auto end = copy_if(execution::par,
                            result.plus_words.begin(),
                            result.plus_words.end(),
                            matched_words.begin(),
                            check);
 
    sort(matched_words.begin(), end);
    end = unique(execution::par,
                      matched_words.begin(),
                      end);
 
    matched_words.erase(end, matched_words.end());
    return {matched_words,
            documents_.at(document_id).status};
}


vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const{
    auto it = document_to_word_freqs_.find(document_id);
    if(it == document_to_word_freqs_.end()){
        static map<string_view, double> word_freqs_empty;
        return word_freqs_empty;
    }

    return it->second;
}

set<int>::const_iterator SearchServer::begin() const {
    return id_list_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return id_list_.end();
}


vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (const auto word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}


[[nodiscard]] bool SearchServer::IsValidWord(const string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        if ((text.size() == 1) || (text[1] == '-'))
            throw invalid_argument("Invalid request"s);
        text = text.substr(1);
    }
    if (!IsValidWord(text))
        throw invalid_argument("Invalid symbols"s);

    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    vector<string_view> plus_words;
    vector<string_view> minus_words;

    for (const string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                minus_words.push_back(query_word.data);
            }
            else {
                plus_words.push_back(query_word.data);
            }
        }
    }

    return {plus_words, minus_words};
}

SearchServer::Query SearchServer::ParseQueryForSeq(string_view text) const {
    
    Query result = ParseQuery(text);

    sort(result.plus_words.begin(), result.plus_words.end());
    auto last_plus_word = 
    unique(result.plus_words.begin(), result.plus_words.end());

    sort(result.minus_words.begin(), result.minus_words.end());
    auto last_minus_word = 
    unique(result.minus_words.begin(), result.minus_words.end());

    return {vector<string_view>(result.plus_words.begin(), last_plus_word),
            vector<string_view>(result.minus_words.begin(), last_minus_word)};
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}