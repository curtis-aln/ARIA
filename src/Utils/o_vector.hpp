#pragma once

#include <vector>


 // add this as a public parent struct to the Obj class/structure
struct o_vec_object
{
    uint32_t o_vec_index = 0;
    bool active = true;

    o_vec_object() = default;
};


template <class Obj>
class o_vector
{
public:
    // this tracks the amount of objects there are active
    int active_objs = 0;
    int resize_buffering = 300; // if the array is full, we add this amount of empty slots to the end of the array 

private:
    // this array contains all the items and is never directly modified
    std::vector<Obj*> array{};

    // this tracks the actual initilised and implemented Objects being used
    uint32_t array_size = 0;

    // this vectorPtr stores all the actual objects on the heap, they are never modified or removed from. only added to
    std::vector<Obj> objectStore{};

    std::vector<uint32_t> free_list{};
    int free_count = 0;


private:
    // Iterator class definition
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Obj*;
        using difference_type = std::ptrdiff_t;
        using pointer = Obj*;
        using reference = Obj&;

        explicit Iterator(o_vector* vec = nullptr, const unsigned index = 0) : vectorPtr(vec), currentIndex(index) {}
        Iterator() : vectorPtr(nullptr) {}

        Iterator& operator++()
        {
            ++currentIndex;
            while (currentIndex < vectorPtr->array_size && !vectorPtr->array[currentIndex]->active)
                ++currentIndex;
            return *this;
        }

        pointer operator*()   const { return vectorPtr->array[currentIndex]; }
        pointer operator->()  const { return vectorPtr->array[currentIndex]; }

        bool operator==(const Iterator& other) const { return currentIndex == other.currentIndex; }
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        o_vector* vectorPtr;
        unsigned currentIndex = 0;
    };


    unsigned getFirstAvalableIteration() const
    {
        unsigned currentIndex = 0;
        while (currentIndex < array_size && !array[currentIndex]->active)
            ++currentIndex;
        return currentIndex;
    }


public:
    explicit o_vector(const std::size_t N)
    {
        objectStore.reserve(N);
        array.reserve(N);
        free_list.reserve(N);
    }

    [[nodiscard]] Iterator begin() const { return Iterator(const_cast<o_vector*>(this), getFirstAvalableIteration()); }
    [[nodiscard]] Iterator end()   const { return Iterator(const_cast<o_vector*>(this), array_size); }
    [[nodiscard]] unsigned size()  const { return active_objs; }


    // used to initilise items inside of objectStore.
    void emplace(Obj item)
    {
        objectStore.emplace_back(item);
        array.push_back(&objectStore.back());
        free_list.push_back(-1);
        ++array_size;
        ++active_objs;
    }

    void smart_resize()
    {
        // Grow when almost full
        if (free_count < resize_buffering / 2)
        {
            const uint32_t old_size = array_size;
            objectStore.reserve(objectStore.size() + resize_buffering);
            for (int i = 0; i < resize_buffering; ++i)
            {
                objectStore.emplace_back();
                array.push_back(&objectStore.back());
                free_list.push_back(old_size + i);
                ++free_count;
            }
            array_size += resize_buffering;
        }

        // Shrink when too much slack — only trim inactive tail slots
        else if (free_count > resize_buffering * 2)
        {
            int trimmed = 0;
            while (trimmed < resize_buffering && array_size > 0 && !array[array_size - 1]->active)
            {
                array.pop_back();
                --array_size;
                // remove the corresponding entry from free_list
                for (int i = 0; i < free_count; ++i)
                {
                    if (free_list[i] == array_size)
                    {
                        free_list[i] = free_list[--free_count];
                        break;
                    }
                }
                ++trimmed;
            }
        }
    }


    Obj* at(const unsigned i) { return array[i]; }


    Obj* add()
    {
        if (free_count == 0)
            return nullptr;

        const unsigned idx = free_list[--free_count];
        array[idx]->active = true;
        ++active_objs;
        return array[idx];
    }


    void remove(Obj* obj) { if (obj->active) remove(obj->id); }
    void remove(const unsigned vector_index)
    {
        if (array[vector_index]->active)
        {
            array[vector_index]->active = false;
            --active_objs;
            free_list[free_count++] = vector_index;
        }
    }

    int      free_slots()   const { return free_count; }
};