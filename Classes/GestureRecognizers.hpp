//
//  GestureRecognizers.hpp
//  SmoothDrawing
//
//  Created by Benny Khoo on 21/10/2015.
//
//

#ifndef GestureRecognizers_hpp
#define GestureRecognizers_hpp

#include <stdio.h>
#include <array>

using namespace cocos2d;

class VelocityCalculator {
    
public:
    using time_point = std::chrono::high_resolution_clock::time_point;
    static constexpr int MaxVelocitySamples = 10;
    static constexpr bool Debug = false;
    
public:
    VelocityCalculator () : _sampleCount(0), _runningVelocitySum(0, 0), _velocitySamples {}, _first(true) {
    }
    
    void reset()
    {
        _sampleCount = 0;
        _runningVelocitySum = Vec2 {0, 0};
        _first = true;
        _velocitySamples = {};
    }
    
    void addLocation(Vec2 location)
    {
        using namespace std::chrono;
        addLocation(location, high_resolution_clock::now());
    }
    
    void addLocation(Vec2 location, time_point timestamp)
    {
        using namespace std::chrono;
        
        if (Debug)
            CCLOG("adding location %.2f %.2f timestamp %.2lld", location.x, location.y, time_point_cast<milliseconds>(timestamp).time_since_epoch().count());
        
        if (!_first) {
            high_resolution_clock::duration timeSinceLastUpdate = (timestamp - _prevTimestamp);
            
            if (Debug)
                CCLOG("time since last update %.2lld", duration_cast<milliseconds>(timeSinceLastUpdate).count());
            
            Vec2 instVelocity = Vec2 {
                (location.x - _prevLocation.x) / duration_cast<milliseconds>(timeSinceLastUpdate).count() * 1000,
                (location.y - _prevLocation.y) / duration_cast<milliseconds>(timeSinceLastUpdate).count() * 1000
            };
            
            int lastSampleIndex = _sampleCount % MaxVelocitySamples;
            Vec2 lastSample = _velocitySamples[lastSampleIndex];
            
            _runningVelocitySum -= lastSample;
            _runningVelocitySum += instVelocity;
            
            _velocitySamples[lastSampleIndex] = instVelocity;
            _sampleCount++;
        }
        else {
            _first = false;
        }
        
        _prevLocation = location;
        _prevTimestamp = timestamp;
    }
    
    Vec2 getLastVelocitySample()
    {
        int lastSampleIndex = _sampleCount % MaxVelocitySamples;
        return _velocitySamples[lastSampleIndex];
    }
    
    int getSampleCount() { return _sampleCount; }
    
    Vec2 getRunningAvgVelocity()
    {
        return _sampleCount >= MaxVelocitySamples
        ? Vec2 { _runningVelocitySum.x / MaxVelocitySamples, _runningVelocitySum.y / MaxVelocitySamples }
        : Vec2 { _runningVelocitySum.x / _sampleCount, _runningVelocitySum.y / _sampleCount };
    }
    
private:
    bool _first;
    time_point _prevTimestamp;
    Vec2 _prevLocation;
    
    std::array<Vec2, MaxVelocitySamples> _velocitySamples;
    int _sampleCount;
    Vec2 _runningVelocitySum;
    
};

class BasicGestureRecognizer : public Ref
{
public:
    enum State { Possible, Began, Changed, Completed, Failed };
    using targetCallBack = std::function<void(BasicGestureRecognizer*)>;

public:
    
    virtual bool init()
    {
        _state = Possible;
        return true;
    }
    
    void setTarget(targetCallBack t) {
        _target = t;
    }
    Vec2 getLocation() { return _location; }
    State getState() { return _state; }
    
    virtual void addWithSceneGraphPriority(EventDispatcher *eventDispatcher, Node *node) = 0;
    
protected:
    targetCallBack _target;
    State _state;
    Vec2 _location;
};

class PanGestureRecognizer : public BasicGestureRecognizer
{
public:
    static constexpr float MinPanDistance = 5.0f;
    
public:
    static PanGestureRecognizer *create()
    {
        PanGestureRecognizer *node = new (std::nothrow) PanGestureRecognizer();
        if (node)
        {
            node->init();
            node->autorelease();
        }
        else
        {
            CC_SAFE_DELETE(node);
        }
        return node;
    }
    
    void addWithSceneGraphPriority(EventDispatcher *eventDispatcher, Node *node)
    {
        auto eventListener = EventListenerTouchOneByOne::create();
        
        eventListener->onTouchBegan = [this] (Touch *touch, Event *event) -> bool {
            _location = touch->getLocation();
            _velocityCalc.reset();
            _velocityCalc.addLocation(_location);
            _beganLocation = touch->getLocation();
            _state = Possible;
            return true;
        };
        
        eventListener->onTouchMoved = [this] (Touch *touch, Event *event) {
            Vec2 location = touch->getLocation();
            _velocityCalc.addLocation(touch->getLocation());
            _location = location;
            
            if (_state == Possible) {
                if ((location - _beganLocation).getLength() > MinPanDistance) {
                    _state = Began;
                    _target(this);
                    return;
                }
            }
            else if (_state == Began) {
                _state = Changed;
            }
            
            if (_state == Changed) {
                _target(this);
            }
        };
        
        eventListener->onTouchEnded = [this] (Touch *touch, Event *event) {
            _location = touch->getLocation();
            if (_state == Changed) {
                _state = Completed;
                _target(this);
            }
        };
        
        eventDispatcher->addEventListenerWithSceneGraphPriority(eventListener, node);
    }
    
    Vec2 getVelocity() { return _velocityCalc.getRunningAvgVelocity(); }
    
private:
    Vec2 _beganLocation;
    VelocityCalculator _velocityCalc;
    
};

class LongPressGestureRecognizer : public BasicGestureRecognizer
{
public:
    static constexpr long long MinimumPressDurationMilliSecs = 500;
    static constexpr float AllowableMovement = 10.0;
    using time_point = std::chrono::high_resolution_clock::time_point;
    
private:
    static constexpr const char *UpdateKey = "LongPressGestureUpdate";

public:
    static LongPressGestureRecognizer *create()
    {
        LongPressGestureRecognizer *node = new (std::nothrow) LongPressGestureRecognizer();
        if (node)
        {
            node->init();
            node->autorelease();
        }
        else
        {
            CC_SAFE_DELETE(node);
        }
        return node;
    }
    
    void addWithSceneGraphPriority(EventDispatcher *eventDispatcher, Node *node)
    {
        reset();
        
        auto eventListener = EventListenerTouchOneByOne::create();

        eventListener->onTouchBegan = [this] (Touch *touch, Event *event) -> bool {
            _state = Began;
            _startLocation = touch->getLocation();
            _startTime = std::chrono::high_resolution_clock::now();
            this->scheduleUpdate();
            return true;
        };
        
        eventListener->onTouchMoved = [this] (Touch *touch, Event *event) {
            
            if (_state == Began || _state == Changed) {
                if ( checkLongPress(touch) ) {
                    _state = Changed;
                    _target(this);
                } else {
                    reset();
                }
            }
        };
        
        eventListener->onTouchEnded = [this] (Touch *touch, Event *event) {
            if (_state == Began || _state == Changed) {
                if ( checkLongPress(touch) ) {
                    _state = Completed;
                    _target(this);
                } else {
                    reset();
                }
            }
        };
        
        eventDispatcher->addEventListenerWithSceneGraphPriority(eventListener, node);
        _node = node;
    }
    
    void reset()
    {
        _state = Possible;
        this->removeUpdate();
    }
    
    bool checkLongPress(Touch *touch)
    {
        using namespace std::chrono;
        
        bool result = false;
        Vec2 location = touch->getLocation();
        float distMoved = (location - _startLocation).getLength();
        auto duration = duration_cast<milliseconds>(high_resolution_clock::now() - _startTime).count();
        
//        CCLOG("long press %.2f %lld", distMoved, duration);
        
        if ( distMoved < AllowableMovement && duration > MinimumPressDurationMilliSecs) {
            result = true;
        }
        
        return result;
    }
    
    void scheduleUpdate()
    {
        CC_ASSERT(_node != nullptr);
        
        // test if gesture is holding on one spot longer than MinimumPressDurationMilliSecs.
        
        _node->schedule([this](float dt) {
            using namespace std::chrono;
            
//            CCLOG("received long gesture update");
            if (_state == Began || _state == Changed) {
                
                auto duration = duration_cast<milliseconds>(high_resolution_clock::now() - _startTime).count();
                if (duration > MinimumPressDurationMilliSecs) {
                    _state = Changed;
                    _target(this);
                    reset();
                }
            }
        }, UpdateKey);
        
    }
    
    void removeUpdate()
    {
//        CCLOG("remove long gesture update");
        if (_node != nullptr) {
            _node->unschedule(UpdateKey);
        }
    }
    
private:
    Vec2 _startLocation;
    time_point _startTime;
    Node *_node;

};

#endif /* GestureRecognizers_hpp */
