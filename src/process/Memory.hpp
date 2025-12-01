#pragma once
#include <vector>
#include <map>
#include <deque>
#include <iostream>
#include <mutex>
#include <cmath>
#include <memory> // REQUIRED for std::shared_ptr

// Represents a Page in the Page Table
struct PageTableEntry {
    int frameNumber = -1; // -1 means not in memory
    bool valid = false;   // true = in RAM, false = in Backing Store
};

class MemoryManager {
private:
    // Config
    size_t memorySize;
    size_t frameSize;
    size_t numFrames;

    std::vector<uint16_t> mainMemory; 

    std::deque<int> freeFrames;

    std::deque<int> activeFrames; 
    std::map<int, std::pair<int, int>> frameOwner; 
    std::map<int, std::map<int, PageTableEntry>> pageTables;
    std::map<int, std::map<int, std::vector<uint16_t>>> backingStore;

    std::atomic<int> numPagedIn{0};
    std::atomic<int> numPagedOut{0};

    std::mutex memMtx;

public:
    MemoryManager(size_t memSize, size_t fSize) : memorySize(memSize), frameSize(fSize) {
        numFrames = memorySize / frameSize;
        mainMemory.resize(memorySize, 0);
        
        // Initialize free frames
        for(size_t i = 0; i < numFrames; i++) {
            freeFrames.push_back(i);
        }
    }

    // Allocate virtual space for process
    void allocateMemory(int pid) {
        std::lock_guard<std::mutex> lock(memMtx);
        pageTables[pid] = std::map<int, PageTableEntry>();
    }

    // Remove process from memory completely
    void deallocateMemory(int pid) {
        std::lock_guard<std::mutex> lock(memMtx);
        
        for (auto& entry : pageTables[pid]) {
            if (entry.second.valid) {
                int frame = entry.second.frameNumber;
                freeFrames.push_back(frame);
                frameOwner.erase(frame);
            }
        }
        pageTables.erase(pid);
        backingStore.erase(pid);
    }

    // Read/Write access
    uint16_t access(int pid, int virtualAddress, bool isWrite, uint16_t writeVal = 0) {
        std::lock_guard<std::mutex> lock(memMtx);

        int pageNum = virtualAddress / frameSize;
        int offset = virtualAddress % frameSize;

        // Ensure page entry exists
        if (pageTables[pid].find(pageNum) == pageTables[pid].end()) {
            pageTables[pid][pageNum] = PageTableEntry();
            backingStore[pid][pageNum] = std::vector<uint16_t>(frameSize, 0);
        }

        PageTableEntry& entry = pageTables[pid][pageNum];

        // PAGE FAULT CHECK
        if (!entry.valid) {
            handlePageFault(pid, pageNum);
        }

        // Physical Address Calculation
        int physAddr = (entry.frameNumber * frameSize) + offset;

        if (isWrite) {
            mainMemory[physAddr] = writeVal;
            return writeVal;
        } else {
            return mainMemory[physAddr];
        }
    }

    // Helper: Calculate total memory usage for logging
    std::string getMemoryUsage() {
        std::lock_guard<std::mutex> lock(memMtx);
        int used = numFrames - freeFrames.size();
        return std::to_string(used) + "/" + std::to_string(numFrames) + " frames";
    }

    size_t getMemorySize() { return memorySize; }
    
    // Calculate used/free based on frames (Thread-safe)
    size_t getUsedMemory() {
        std::lock_guard<std::mutex> lock(memMtx);
        return (numFrames - freeFrames.size()) * frameSize;
    }

    size_t getFreeMemory() {
        std::lock_guard<std::mutex> lock(memMtx);
        return freeFrames.size() * frameSize;
    }

    int getPagedInCount() { return numPagedIn.load(); }
    int getPagedOutCount() { return numPagedOut.load(); }
private:
    void handlePageFault(int pid, int pageNum) {
        // std::cout << "[Memory] Page Fault for Process " << pid << " Page " << pageNum << std::endl;
        numPagedIn++;

        int frameToUse = -1;

        if (!freeFrames.empty()) {
            // Case 1: Free frame available
            frameToUse = freeFrames.front();
            freeFrames.pop_front();
        } else {
            // Case 2: Replacement needed (FIFO)
            if (activeFrames.empty()) throw std::runtime_error("Memory Error: No frames available");
            
            frameToUse = activeFrames.front();
            activeFrames.pop_front();

            // Evict current owner
            std::pair<int, int> owner = frameOwner[frameToUse];
            evictPage(owner.first, owner.second, frameToUse);
        }

        // Load data from Backing Store to RAM
        std::vector<uint16_t>& data = backingStore[pid][pageNum];
        int startAddr = frameToUse * frameSize;
        for (size_t i = 0; i < frameSize; i++) {
            mainMemory[startAddr + i] = data[i];
        }

        // Update Page Table
        pageTables[pid][pageNum].frameNumber = frameToUse;
        pageTables[pid][pageNum].valid = true;

        // Update Usage Tracking
        activeFrames.push_back(frameToUse);
        frameOwner[frameToUse] = {pid, pageNum};
    }

    void evictPage(int pid, int pageNum, int frameNum) {
        numPagedOut++;

        // Copy data from RAM to Backing Store
        int startAddr = frameNum * frameSize;
        std::vector<uint16_t> data(frameSize);
        
        for (size_t i = 0; i < frameSize; i++) {
            data[i] = mainMemory[startAddr + i];
        }
        
        backingStore[pid][pageNum] = data;

        // Mark Invalid in Page Table
        if (pageTables.count(pid) && pageTables[pid].count(pageNum)) {
            pageTables[pid][pageNum].valid = false;
            pageTables[pid][pageNum].frameNumber = -1;
        }
    }
};

// Global instance declaration
extern std::shared_ptr<MemoryManager> globalMem;