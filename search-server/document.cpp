#include "document.h"

using namespace std;

std::ostream& operator<<(ostream& out, const Document& document) {
    cout << "{ document_id = " << document.id << ", relevance = " << document.relevance << ", rating = " << document.rating << " }";
    return out;
}