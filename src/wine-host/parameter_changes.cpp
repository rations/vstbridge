/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @file parameter_changes.cpp
 * @brief Implementation of VST3 parameter change interfaces
 */

#include "parameter_changes.h"
#include <cstring>

namespace vst3bridge {

// FUID definitions for queryInterface
static const char IParamValueQueue_IID[16] = {};
static const char IParameterChanges_IID[16] = {};
static const char FUnknown_IID[16] = {};

// ============================================================================
// ParamValueQueue Implementation
// ============================================================================

ParamValueQueue::ParamValueQueue()
    : parameterId_(0)
    , refCount_(1) {
}

ParamValueQueue::~ParamValueQueue() {
}

Steinberg::tresult PLUGIN_API ParamValueQueue::queryInterface(const Steinberg::TUID _iid, void** obj) {
    if (obj == nullptr) {
        return Steinberg::kInvalidArgument;
    }
    
    *obj = nullptr;
    
    if (std::memcmp(_iid, IParamValueQueue_IID, sizeof(Steinberg::TUID)) == 0 ||
        std::memcmp(_iid, FUnknown_IID, sizeof(Steinberg::TUID)) == 0) {
        *obj = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
        addRef();
        return Steinberg::kResultOk;
    }
    
    return Steinberg::kNoInterface;
}

Steinberg::uint32 PLUGIN_API ParamValueQueue::addRef() {
    return ++refCount_;
}

Steinberg::uint32 PLUGIN_API ParamValueQueue::release() {
    Steinberg::uint32 count = --refCount_;
    if (count == 0) {
        delete this;
    }
    return count;
}

Steinberg::Vst::ParamID PLUGIN_API ParamValueQueue::getParameterId() {
    return parameterId_;
}

int32_t PLUGIN_API ParamValueQueue::getPointCount() {
    return static_cast<int32_t>(points_.size());
}

Steinberg::tresult PLUGIN_API ParamValueQueue::getPoint(int32_t index, 
                                                         int32_t& sampleOffset, 
                                                         Steinberg::Vst::ParamValue& value) {
    if (index < 0 || index >= static_cast<int32_t>(points_.size())) {
        return Steinberg::kInvalidArgument;
    }
    
    const Point& point = points_[static_cast<size_t>(index)];
    sampleOffset = point.sampleOffset;
    value = point.value;
    
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API ParamValueQueue::addPoint(int32_t sampleOffset, 
                                                         Steinberg::Vst::ParamValue value, 
                                                         int32_t& index) {
    // Find insertion point to keep array sorted by sampleOffset
    auto it = std::lower_bound(points_.begin(), points_.end(), sampleOffset,
        [](const Point& p, int32_t offset) {
            return p.sampleOffset < offset;
        });
    
    // Check if we already have a point at this sample offset
    if (it != points_.end() && it->sampleOffset == sampleOffset) {
        // Update existing point
        it->value = value;
        index = static_cast<int32_t>(std::distance(points_.begin(), it));
    } else {
        // Insert new point
        Point newPoint;
        newPoint.sampleOffset = sampleOffset;
        newPoint.value = value;
        it = points_.insert(it, newPoint);
        index = static_cast<int32_t>(std::distance(points_.begin(), it));
    }
    
    return Steinberg::kResultOk;
}

void ParamValueQueue::setParameterId(Steinberg::Vst::ParamID id) {
    parameterId_ = id;
}

void ParamValueQueue::clear() {
    points_.clear();
}

bool ParamValueQueue::addPoint(int32_t sampleOffset, Steinberg::Vst::ParamValue value) {
    int32_t index;
    return addPoint(sampleOffset, value, index) == Steinberg::kResultOk;
}

// ============================================================================
// ParameterChanges Implementation
// ============================================================================

ParameterChanges::ParameterChanges()
    : refCount_(1) {
}

ParameterChanges::~ParameterChanges() {
    clear();
}

Steinberg::tresult PLUGIN_API ParameterChanges::queryInterface(const Steinberg::TUID _iid, void** obj) {
    if (obj == nullptr) {
        return Steinberg::kInvalidArgument;
    }
    
    *obj = nullptr;
    
    if (std::memcmp(_iid, IParameterChanges_IID, sizeof(Steinberg::TUID)) == 0 ||
        std::memcmp(_iid, FUnknown_IID, sizeof(Steinberg::TUID)) == 0) {
        *obj = static_cast<Steinberg::Vst::IParameterChanges*>(this);
        addRef();
        return Steinberg::kResultOk;
    }
    
    return Steinberg::kNoInterface;
}

Steinberg::uint32 PLUGIN_API ParameterChanges::addRef() {
    return ++refCount_;
}

Steinberg::uint32 PLUGIN_API ParameterChanges::release() {
    Steinberg::uint32 count = --refCount_;
    if (count == 0) {
        delete this;
    }
    return count;
}

int32_t PLUGIN_API ParameterChanges::getParameterCount() {
    return static_cast<int32_t>(queues_.size());
}

Steinberg::Vst::IParamValueQueue* PLUGIN_API ParameterChanges::getParameterData(int32_t index) {
    if (index < 0 || index >= static_cast<int32_t>(queues_.size())) {
        return nullptr;
    }
    
    Steinberg::Vst::IParamValueQueue* queue = queues_[static_cast<size_t>(index)];
    queue->addRef();  // Caller must release
    return queue;
}

Steinberg::Vst::IParamValueQueue* PLUGIN_API ParameterChanges::addParameterData(
    const Steinberg::Vst::ParamID& id, 
    int32_t& index) {
    
    // Check if we already have a queue for this parameter
    auto it = queueMap_.find(id);
    if (it != queueMap_.end()) {
        // Find index of existing queue
        for (size_t i = 0; i < queues_.size(); ++i) {
            if (queues_[i] == it->second) {
                index = static_cast<int32_t>(i);
                it->second->addRef();
                return it->second;
            }
        }
    }
    
    // Create new queue
    ParamValueQueue* newQueue = new ParamValueQueue();
    newQueue->setParameterId(id);
    
    index = static_cast<int32_t>(queues_.size());
    queues_.push_back(newQueue);
    queueMap_[id] = newQueue;
    
    newQueue->addRef();
    return newQueue;
}

void ParameterChanges::clear() {
    // Release all references
    for (auto* queue : queues_) {
        queue->release();
    }
    queues_.clear();
    queueMap_.clear();
}

bool ParameterChanges::addParameterChange(Steinberg::Vst::ParamID id, 
                                         int32_t sampleOffset, 
                                         Steinberg::Vst::ParamValue value) {
    int32_t index;
    Steinberg::Vst::IParamValueQueue* queue = addParameterData(id, index);
    if (!queue) {
        return false;
    }
    
    int32_t pointIndex;
    Steinberg::tresult result = queue->addPoint(sampleOffset, value, pointIndex);
    queue->release();
    
    return result == Steinberg::kResultOk;
}

void ParameterChanges::reserve(int32_t count) {
    queues_.reserve(static_cast<size_t>(count));
}

// ============================================================================
// ParameterChangesBuilder Implementation
// ============================================================================

ParameterChangesBuilder::ParameterChangesBuilder() {
}

ParameterChangesBuilder::~ParameterChangesBuilder() {
}

void ParameterChangesBuilder::addChange(Steinberg::Vst::ParamID paramId, 
                                        int32_t sampleOffset, 
                                        Steinberg::Vst::ParamValue value) {
    changes_[paramId].push_back(std::make_pair(sampleOffset, value));
}

ParameterChanges* ParameterChangesBuilder::build() {
    if (changes_.empty()) {
        return nullptr;
    }
    
    ParameterChanges* result = new ParameterChanges();
    result->reserve(static_cast<int32_t>(changes_.size()));
    
    for (const auto& entry : changes_) {
        Steinberg::Vst::ParamID paramId = entry.first;
        const auto& points = entry.second;
        
        for (const auto& point : points) {
            result->addParameterChange(paramId, point.first, point.second);
        }
    }
    
    return result;
}

void ParameterChangesBuilder::clear() {
    changes_.clear();
}

int32_t ParameterChangesBuilder::getParameterCount() const {
    return static_cast<int32_t>(changes_.size());
}

} // namespace vst3bridge