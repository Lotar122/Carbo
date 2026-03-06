#pragma once

#include <cstddef>
#include <vector>
#include <deque>
#include <cassert>

namespace Carbo 
{
    template<typename T>
    class ECSAllocator
    {
        struct DataWrapper 
        {
            T data;
            bool valid = true;

            DataWrapper(T& _data) : data(_data) {};
            DataWrapper(T&& _data) : data(std::move(_data)) {};

            DataWrapper(T& _data, bool _valid) : data(_data), valid(_valid) {};
            DataWrapper(T&& _data, bool _valid) : data(std::move(_data)), valid(_valid) {};
        };

        std::vector<DataWrapper> data;
        std::deque<size_t> freeList;
    public:
        void free(size_t index)
        {
            data[index].valid = false;
            freeList.push_back(index);
        }

        size_t allocate(T& _data)
        {
            size_t index = -1;

            if(!freeList.empty())
            {
                data[freeList.front()].data = _data;
                data[freeList.front()].valid = true;

                index = freeList.front();

                freeList.pop_front();
            }
            else
            {
                data.emplace_back(_data);
                
                index = data.size() - 1;
            }

            return index;
        }

        size_t allocate(T&& _data)
        {
            size_t index = -1;

            if(!freeList.empty())
            {
                data[freeList.front()].data = std::move(_data);
                data[freeList.front()].valid = true;

                index = freeList.front();

                freeList.pop_front();
            }
            else
            {
                data.emplace_back(std::move(_data));

                index = data.size() - 1;
            }

            return index;
        }

        template<typename... Args>
        size_t allocate(Args&&... args)
        {
            size_t index = -1;

            if(!freeList.empty())
            {
                data[freeList.front()].data = T(std::forward<Args>(args)...);
                data[freeList.front()].valid = true;

                index = freeList.front();

                freeList.pop_front();
            }
            else
            {
                data.emplace_back(std::move(T(std::forward<Args>(args)...)));

                index = data.size() - 1;
            }

            return index;
        }

        size_t allocate()
        {
            size_t index = -1;

            if(!freeList.empty())
            {
                data[freeList.front()].data = T();
                data[freeList.front()].valid = true;

                index = freeList.front();

                freeList.pop_front();
            }
            else
            {
                data.emplace_back(std::move(T()));

                index = data.size() - 1;
            }

            return index;
        }

        inline T& at(size_t index)
        {
            assert(data[index].valid);
            return data[index].data;
        }

        inline void reset()
        {
            data.clear();
        }
    };
}