#include "string_processing.h"
#include <cstdint>

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> result;
    text.remove_prefix(min(text.size(), text.find_first_not_of(' ')));
    const int64_t pos_end = text.npos;

    while (!text.empty()) {
        int64_t space = text.find(' ');
        result.push_back(space == pos_end ? text : text.substr(0, space));
        text.remove_prefix(min(text.size(), text.find_first_not_of(' ', space)));
    }
    return result;
}