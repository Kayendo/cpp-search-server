#pragma once
 
#include "document.h"
#include <iostream>
 
template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
    : first_(begin)
    , last_(end)
    , size_(distance(first_, last_)) {
    }
 
    Iterator begin() const {
        return first_;
    }
 
    Iterator end() const {
        return last_;
    }
 
    size_t size() const {
        return size_;
    }
 

 
private:
    Iterator first_, last_;
    size_t size_;    
};

template <typename Iterator>
std::ostream& std::operator<<(ostream& out, IteratorRange<Iterator> range) {
    for (Iterator i = range.begin(); i < range.end(); ++i) {
        cout << *i;
    }
    return out;
}
 
template <typename Iterator>
class Paginator {
public:
    std::vector<IteratorRange<Iterator>> v;
    Paginator(Iterator begin, Iterator end, size_t page_size)
    : first_(begin)
    , last_(end)
    , page_size_(page_size) {
        for (int i = 0; i < distance(begin, end); i+=page_size) {
            v.push_back(IteratorRange(begin + i, min(begin + i + page_size, end)));
        }
    }
 
    auto begin() const {
        return v.begin();
    }
 
    auto end() const {
        return v.end();
    }
 
    size_t size() const {
        return v.size();
    }
 
private:
    Iterator first_, last_;
    size_t page_size_;
};
 
template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
