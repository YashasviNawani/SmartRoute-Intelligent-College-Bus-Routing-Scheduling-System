#ifndef HASH_TABLE_H
#define HASH_TABLE_H
#include <vector>
#include <list>
#include <utility>
template <typename T>
class HashTable {
private:
    static const int TABLE_SIZE = 101;
    std::vector<std::list<std::pair<int, T>>> buckets;

    int hashFunction(int key) const {
        return key % TABLE_SIZE;
    }
    
public:
    HashTable() : buckets(TABLE_SIZE) {}

    void insert(int key, const T& value) {
        int idx = hashFunction(key);
        for (auto& entry : buckets[idx]) {
            if (entry.first == key) { entry.second = value; return; }  // update existing
        }
        buckets[idx].push_back({key, value});
    }

    bool remove(int key) {
        int idx = hashFunction(key);
        auto& chain = buckets[idx];
        for (auto it = chain.begin(); it != chain.end(); ++it) {
            if (it->first == key) { chain.erase(it); return true; }
        }
        return false;
    }

    T* find(int key) {
        int idx = hashFunction(key);
        for (auto& entry : buckets[idx]) {
            if (entry.first == key) return &entry.second;
        }
        return nullptr;
    }

    bool exists(int key) const {
        int idx = hashFunction(key);
        for (const auto& entry : buckets[idx]) {
            if (entry.first == key) return true;
        }
        return false;
    }

    std::vector<T> getAll() const {
        std::vector<T> all;
        for (const auto& chain : buckets)
            for (const auto& entry : chain)
                all.push_back(entry.second);
        return all;
    }

    int size() const {
        int count = 0;
        for (const auto& chain : buckets) count += static_cast<int>(chain.size());
        return count;
    }
};

#endif
