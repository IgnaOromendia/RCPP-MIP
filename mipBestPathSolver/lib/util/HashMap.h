#ifndef HASHMAP_H
#define HASHMAP_H

#include <glib.h>
#include <vector>

class HashMap {
    public:

    HashMap() noexcept;

    // Para que no se copie
    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    HashMap(HashMap&& other) noexcept;

    ~HashMap(); 

    HashMap& operator=(HashMap&& other) noexcept;

    void add(int key, int value);

    const std::vector<int>& get(int key) const;

    bool contains(int key) const;

    std::size_t size() const;

    bool empty() const;

    private:
        GHashTable* _map = nullptr;

        static void vec_destructor(gpointer p);

        const std::vector<int>* find(int key) const;

        std::vector<int>& ensure(int key);
};

#endif
