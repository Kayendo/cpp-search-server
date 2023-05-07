#include "remove_duplicates.h"

#include <string>
#include <map>
#include <algorithm>

using namespace std;

void RemoveDuplicates(SearchServer &search_server) {
	set<set<string>> word_freq;
	set<int> ides;
	for (const int id : search_server) {
		set<string> words;
		transform(search_server.GetWordFrequencies(id).begin(), search_server.GetWordFrequencies(id).end(), inserter(words, words.begin()), 
                  [](const pair<string, double> &word_freqs) {
					return word_freqs.first;
				});
        
		if (word_freq.count(words)) {
           ides.insert(id);
		} else {
			word_freq.insert(words);
		}
	}

	for (int id : ides) {
		search_server.RemoveDocument(id);
		cout << "Found duplicate document id "s << id << endl;
	}
}