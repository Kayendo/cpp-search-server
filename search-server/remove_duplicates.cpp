#include "remove_duplicates.h"

#include <string>
#include <map>
#include <algorithm>

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    map<set<string>, int> word_freq;
    set<int> ides;
    for (const int id : search_server) {
        map<string, double> word_frequencies = search_server.GetWordFrequencies(id);
        set<string> words;
        for (const auto& word : word_frequencies) {
            words.insert(word.first);
        }
        if (word_freq.count(words)) {
            int cur_id = word_freq.at(words);
            int max_id = max(cur_id, id);
            ides.insert(max_id);
            int min_id = min(cur_id, id);
            word_freq[words] = min_id;
        }
        else {
           word_freq.insert(pair{ words, id });
        }    
    }

    for (const int id : ides) {
        cout << "Found duplicate document id " << id << endl;
        search_server.RemoveDocument(id);
    }
}