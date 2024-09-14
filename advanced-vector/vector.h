#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    Vector(Vector&& other) noexcept {
        Swap(other);
    }


    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return const_iterator(data_.GetAddress());
    }
    const_iterator end() const noexcept {
        return const_iterator(data_.GetAddress() + size_);
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }



    template <typename... N>
    iterator Emplace(const_iterator pos, N&&... arg) {
        assert(pos >= begin() && pos <= end());
        if (pos == end()) {
            return &EmplaceBack(std::forward<N>(arg)...);
        }
        if (size_ == Capacity()) {
            size_t i = static_cast<size_t>(pos - begin());
            RawMemory<T> temp_vec{ size_ == 0 ? 1 : size_ * 2 };
            new (temp_vec + i) T(std::forward<N>(arg)...);
            try {
                MoveOrCopy(data_.GetAddress(), i, temp_vec.GetAddress());
            }
            catch (...) {
                temp_vec[i].~T();
                throw;
            }

            try {
                MoveOrCopy(data_ + i, size_ - i, temp_vec + (i + 1));
            }
            catch (...) {
                std::destroy_n(temp_vec.GetAddress(), i + 1);
                throw;
            }
            data_.Swap(temp_vec);

            ++size_;
            return begin() + i;
        }
        else {
            size_t i = static_cast<size_t>(pos - begin());
            T temp(std::forward<N>(arg)...);
            new (end()) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + i, end() - 1, end());
            data_[i] = std::move(temp);

            ++size_;
            return begin() + i;
        }
    }


    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= begin() && pos < end());
        size_t space = pos - begin();
        std::move(begin() + space + 1, end(), begin() + space);
        PopBack();
        return begin() + space;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Resize(size_t new_size) {
        if (size_ == new_size) {
            return;
        }
        if (size_ > new_size) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }

    }

    template <typename... N>
    T& EmplaceBack(N&&... arg) {
        T* t = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> temp_data(size_ == 0 ? 1 : size_ * 2);
            t = new (temp_data + size_) T(std::forward<N>(arg)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, temp_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, temp_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(temp_data);
        }
        else {
            t = new (data_ + size_) T(std::forward<N>(arg)...);
        }
        ++size_;
        return *t;
    }

    void PushBack(const T& value) {
        EmplaceBack(std::forward<const T&>(value));
    }

    void PushBack(T&& value) {
        EmplaceBack(std::forward<T&&>(value));
    }

    void PopBack() noexcept {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }



    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> temp_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, temp_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, temp_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(temp_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (data_.Capacity() < rhs.size_) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;

            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

private:
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    void MoveOrCopy(T* from_vec, size_t size, T* target_vec) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from_vec, size, target_vec);
        }
        else {
            std::uninitialized_copy_n(from_vec, size, target_vec);
        }
        std::destroy_n(from_vec, size);
    }


    RawMemory<T> data_;
    size_t size_ = 0;
};