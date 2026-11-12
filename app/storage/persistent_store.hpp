#pragma once

#include "store.hpp"
#include "word_storage.hpp"

namespace manipulator {

// Portable two-copy policy. WordStorage supplies physical erase/program/read.
class PersistentConfigurationStore final : public ConfigurationStore {
public:

    explicit PersistentConfigurationStore(WordStorage& storage) : storage_(storage) {}

    bool load(SavedConfiguration&) override;
    bool save(const SavedConfiguration&) override;

private:

    static bool newer(uint32_t a, uint32_t b);
    WordStorage& storage_;
};

}
