//
//  PositionalAudioRingBuffer.h
//  hifi
//
//  Created by Stephen Birarda on 6/5/13.
//  Copyright (c) 2013 HighFidelity, Inc. All rights reserved.
//

#ifndef __hifi__PositionalAudioRingBuffer__
#define __hifi__PositionalAudioRingBuffer__

#include <vector>
#include <glm/gtx/quaternion.hpp>

#include "AudioRingBuffer.h"

class PositionalAudioRingBuffer : public AudioRingBuffer {
public:
    enum Type {
        Microphone,
        Injector
    };
    
    PositionalAudioRingBuffer(PositionalAudioRingBuffer::Type type);
    ~PositionalAudioRingBuffer();
    
    int parseData(unsigned char* sourceBuffer, int numBytes);
    int parsePositionalData(unsigned char* sourceBuffer, int numBytes);
    int parseListenModeData(unsigned char* sourceBuffer, int numBytes);
    
    bool shouldBeAddedToMix(int numJitterBufferSamples);
    
    bool willBeAddedToMix() const { return _willBeAddedToMix; }
    void setWillBeAddedToMix(bool willBeAddedToMix) { _willBeAddedToMix = willBeAddedToMix; }
    
    PositionalAudioRingBuffer::Type getType() const { return _type; }
    const glm::vec3& getPosition() const { return _position; }
    const glm::quat& getOrientation() const { return _orientation; }
    
protected:
    // disallow copying of PositionalAudioRingBuffer objects
    PositionalAudioRingBuffer(const PositionalAudioRingBuffer&);
    PositionalAudioRingBuffer& operator= (const PositionalAudioRingBuffer&);
    
    PositionalAudioRingBuffer::Type _type;
    glm::vec3 _position;
    glm::quat _orientation;
    bool _willBeAddedToMix;
};

#endif /* defined(__hifi__PositionalAudioRingBuffer__) */
