#pragma once
#ifndef __KOS__MEMORY__MEMORY_H
#define __KOS__MEMORY__MEMORY_H

#include <common/types.hpp>
using namespace kos::common;

#define PAGE_SIZE 4096ULL
#define PAGE_SIZE_SHIFT 12ULL


typedef uintptr_t phys_addr_t;
typedef uintptr_t virt_addr_t;

#endif // __KOS__MEMORY__MEMORY_H