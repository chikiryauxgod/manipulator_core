#include "flash.hpp"

#include "stm32f4xx_hal.h"

namespace manipulator::stm32::drivers {
namespace {

constexpr uint32_t addresses[] = {0x08010000U, 0x08020000U};
constexpr uint32_t sectors[] = {FLASH_SECTOR_4, FLASH_SECTOR_5};
constexpr std::size_t capacity_words = 64U * 1024U / sizeof(uint32_t);

bool valid(unsigned slot, const void* words, std::size_t count)
{
    return slot < 2 && words && count > 0 && count <= capacity_words;
}

}

bool ConfigurationFlash::read(unsigned slot, uint32_t* words, std::size_t count) const
{
    if (!valid(slot, words, count)) {
        return false;
    }
    const auto* source = reinterpret_cast<const volatile uint32_t*>(addresses[slot]);
    for (std::size_t i = 0; i < count; ++i) {
        words[i] = source[i];
    }
    return true;
}

bool ConfigurationFlash::write(unsigned slot, const uint32_t* words, std::size_t count)
{
    if (!valid(slot, words, count) || HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }

    FLASH_EraseInitTypeDef erase{};
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = sectors[slot];
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t error;
    bool ok = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;
    for (std::size_t i = 0; ok && i < count; ++i) {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              addresses[slot] + sizeof(uint32_t) * i, words[i]) == HAL_OK;
    }
    HAL_FLASH_Lock();

    const bool cache_enabled = (FLASH->ACR & FLASH_ACR_DCEN) != 0;
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET();
    if (cache_enabled) {
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
    return ok;
}

}
