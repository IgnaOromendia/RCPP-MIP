#include "../../lib/util/HashMap.h"

HashMap::HashMap() noexcept {
    this->_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, nullptr, &HashMap::vec_destructor);
}

HashMap::HashMap(HashMap&& other) noexcept {
    this->_map = other._map;
    other._map = nullptr;
}

HashMap::~HashMap() { 
    if (_map) g_hash_table_destroy(_map); 
}
HashMap& HashMap::operator=(HashMap&& other) noexcept {
    if (this != &other) {
        if (_map) g_hash_table_destroy(_map);
        _map = other._map;
        other._map = nullptr;
    }
    return *this;
}

void HashMap::add(int key, int value) {
    std::vector<int>& vec = ensure(key);
    vec.push_back(value);
}

const std::vector<int>& HashMap::get(int key) const {
    static const std::vector<int> empty;
    const std::vector<int>* value = find(key);
    return value ? *value : empty;
}

bool HashMap::contains(int key) const {
    GHashTable* mut = const_cast<GHashTable*>(_map);
    return g_hash_table_contains(mut, GINT_TO_POINTER(key));
}

std::size_t HashMap::size() const { 
    return static_cast<std::size_t>(g_hash_table_size(_map)); 
}

bool HashMap::empty() const { 
    return size() == 0; 
}

void HashMap::vec_destructor(gpointer p) {
    delete static_cast<std::vector<int>*>(p);
}

const std::vector<int>* HashMap::find(int key) const {
    GHashTable* mut = const_cast<GHashTable*>(_map);
    return static_cast<const std::vector<int>*>(g_hash_table_lookup(mut, GINT_TO_POINTER(key)));
}

std::vector<int>& HashMap::ensure(int key) {
    auto* vec = const_cast<std::vector<int>*>(find(key));
    if (!vec) {
        vec = new std::vector<int>();
        g_hash_table_insert(_map, GINT_TO_POINTER(key), vec);
    }
    return *vec;
}
