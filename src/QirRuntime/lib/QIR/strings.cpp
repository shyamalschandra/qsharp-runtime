// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <assert.h>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>

#include "quantum__rt.hpp"
#include "qirTypes.hpp"

std::unordered_map<std::string, QirString*>& AllocatedStrings()
{
    static std::unordered_map<std::string, QirString*> allocatedStrings;
    return allocatedStrings;
}

static QirString* CreateOrReuseAlreadyAllocated(std::string&& str)
{
    QirString* qstr = nullptr;
    auto alreadyAllocated = AllocatedStrings().find(str);
    if (alreadyAllocated != AllocatedStrings().end())
    {
        qstr = alreadyAllocated->second;
        assert(qstr->str.compare(str) == 0);
        qstr->refCount++;
    }
    else
    {
        qstr = new QirString(std::move(str));
        AllocatedStrings()[qstr->str] = qstr;
    }
    return qstr;
}

QirString::QirString(std::string&& str)
    : refCount(1)
    , str(std::move(str))
{
}

extern "C"
{
    // Creates a string from an array of UTF-8 bytes.
    QirString* quantum__rt__string_create(const char* bytes) // NOLINT
    {
        return CreateOrReuseAlreadyAllocated(std::string(bytes));
    }

    // Indicates that a new reference has been added.
    void quantum__rt__string_reference(QirString* qstr) // NOLINT
    {
        assert(qstr->refCount > 0);
        ++qstr->refCount;
    }

    // Indicates that an existing reference has been removed and potentially releases the string.
    void quantum__rt__string_unreference(QirString* qstr) // NOLINT
    {
        assert(qstr->refCount > 0);
        const long rc = --qstr->refCount;
        if (rc == 0)
        {
            auto allocated = AllocatedStrings().find(qstr->str);
            assert(allocated != AllocatedStrings().end());
            // TODO: amortize map cleanup across multiple iterations
            AllocatedStrings().erase(allocated);
            delete qstr;
        }
    }

    // Creates a new string that is the concatenation of the two argument strings.
    QirString* quantum__rt__string_concatenate(QirString* left, QirString* right) // NOLINT
    {
        return CreateOrReuseAlreadyAllocated(left->str + right->str);
    }

    // Returns true if the two strings are equal, false otherwise.
    bool quantum__rt__string_equal(QirString* left, QirString* right) // NOLINT
    {
        assert((left == right) == (left->str.compare(right->str) == 0));
        return left == right;
    }

    // Returns a string representation of the integer.
    QirString* quantum__rt__int_to_string(int64_t value) // NOLINT
    {
        return CreateOrReuseAlreadyAllocated(std::to_string(value));
    }

    // Returns a string representation of the double.
    QirString* quantum__rt__double_to_string(double value) // NOLINT
    {
        std::ostringstream oss;
        oss.precision(std::numeric_limits<double>::max_digits10);
        oss << std::fixed << value;
        std::string str = oss.str();

        // Remove padding zeros from the decimal part (relies on the fact that the output for integers always contains
        // period).
        std::size_t pos1 = str.find_last_not_of("0");
        if (pos1 != std::string::npos)
        {
            str.erase(pos1 + 1);
        }
        // For readability don't end with "." -- always have at least one digit in the decimal part.
        if (str[str.size() - 1] == '.')
        {
            str.append("0");
        }

        return CreateOrReuseAlreadyAllocated(std::move(str));
    }

    // Returns a string representation of the Boolean.
    QirString* quantum__rt__bool_to_string(bool value) // NOLINT
    {
        std::string str = value ? "true" : "false";
        return CreateOrReuseAlreadyAllocated(std::move(str));
    }

    // Returns a string representation of the Pauli.
    QirString* quantum__rt__pauli_to_string(PauliId pauli) // NOLINT
    {
        switch (pauli)
        {
        case PauliId_I:
            return quantum__rt__string_create("PauliI");
        case PauliId_X:
            return quantum__rt__string_create("PauliX");
        case PauliId_Y:
            return quantum__rt__string_create("PauliY");
        case PauliId_Z:
            return quantum__rt__string_create("PauliZ");
        }
    }

    // Returns a string representation of the range.
    QirString* quantum__rt__range_to_string(const QirRange& range) // NOLINT
    {
        std::ostringstream oss;
        oss << range.start << "..";
        if (range.step != 1)
        {
          oss << range.step << "..";
        }
        oss << range.end;

        return quantum__rt__string_create(oss.str().c_str());
    }

    // Implemented in delegated.cpp:
    // QirString* quantum__rt__qubit_to_string(QUBIT* qubit); // NOLINT

    // Returns a string representation of the big integer.
    // TODO QirString* quantum__rt__bigint_to_string(QirBigInt*); // NOLINT
}