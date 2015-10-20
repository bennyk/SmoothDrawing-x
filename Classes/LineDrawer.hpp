//
//  LineDrawer.hpp
//  SmoothDrawing
//
//  Created by Benny Khoo on 18/10/2015.
//
//

#ifndef LineDrawer_hpp
#define LineDrawer_hpp

#include <stdio.h>

using namespace cocos2d;

struct LinePoint {
    Vec2 pos;
    float width;
    
    LinePoint (Vec2 p, float w) : pos(p), width(w) {}
    LinePoint() : pos {0, 0}, width {1.0} {}
};

class LineDrawer : public Node {
    
public:
    static LineDrawer *create()
    {
        LineDrawer *node = new (std::nothrow) LineDrawer();
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
    
    LineDrawer () : _overdraw(.5), _enableLineSmoothing(true) {}
    ~LineDrawer() {
        if (_renderTexture != nullptr)
            _renderTexture->release();
    }
    
    virtual bool init()
    {
        auto eventListener = EventListenerTouchOneByOne::create();
        eventListener->onTouchBegan = CC_CALLBACK_2(LineDrawer::onTouchBegan, this);
        eventListener->onTouchMoved = CC_CALLBACK_2(LineDrawer::onTouchMoved, this);
        eventListener->onTouchEnded = CC_CALLBACK_2(LineDrawer::onTouchEnded, this);
        eventListener->onTouchCancelled = CC_CALLBACK_2(LineDrawer::onTouchCancelled, this);
        this->getEventDispatcher()->addEventListenerWithSceneGraphPriority(eventListener, this);
        
        Size size = Director::getInstance()->getWinSize();
        _renderTexture = RenderTexture::create(size.width, size.height, Texture2D::PixelFormat::RGBA8888);
        _renderTexture->retain();
        
        _renderTexture->clear(1.0, 1.0, 1.0, 1.0);
        _renderTexture->setAnchorPoint(Vec2 {0, 0});
        _renderTexture->setPosition(Vec2 {size.width * .5f, size.height * .5f});
        this->addChild(_renderTexture);
        
        return true;
    }
    
    
    void startNewLine(Vec2 point, float size)
    {
        _connectingLine = false;
        addPoint(point, size);
    }
    void addPoint(Vec2 point, float size)
    {
        _points.push_back(LinePoint(point, size));
    }
    void endLine(Vec2 point, float size)
    {
        addPoint(point, size);
        _finishingLine = true;
    }
    
    float extractSize()
    {
        return 1.0;
    }
    
    static void triangulateRect(Vec2 A, Vec2 B, Vec2 C, Vec2 D, Color4F color, std::vector<V3F_C4B_T2F> &vertices, std::vector<unsigned short> &indices, float z = 0)
    {
        triangulateRect(A, color, B, color, C, color, D, color, vertices, indices, z);
    }
    
    static void triangulateRect(Vec2 A, Color4F a, Vec2 B, Color4F b, Vec2 C, Color4F c, Vec2 D, Color4F d, std::vector<V3F_C4B_T2F> &vertices, std::vector<unsigned short> &indices, float z)
    {
//        CCLOG("triangulate rect (%.2f %.2f) (%.2f %.2f) (%.2f %.2f) (%.2f %.2f)", A.x, A.y, B.x, B.y, C.x, C.y, D.x, D.y);
        
        auto startIndex = vertices.size();
        
        vertices.push_back(V3F_C4B_T2F {Vec3 {A.x, A.y, z}, Color4B {a}, Tex2F {}});
        vertices.push_back(V3F_C4B_T2F {Vec3 {B.x, B.y, z}, Color4B {b}, Tex2F {}});
        vertices.push_back(V3F_C4B_T2F {Vec3 {C.x, C.y, z}, Color4B {c}, Tex2F {}});
        vertices.push_back(V3F_C4B_T2F {Vec3 {D.x, D.y, z}, Color4B {d}, Tex2F {}});
        
        indices.push_back(startIndex);     //A
        indices.push_back(startIndex + 1); //B
        indices.push_back(startIndex + 2); //C
        
        indices.push_back(startIndex + 1); //B
        indices.push_back(startIndex + 2); //C
        indices.push_back(startIndex + 3); //D
    }
    
    virtual void draw(Renderer *renderer, const Mat4 &transform, uint32_t flags)
    {
        _renderTexture->begin();
        
        setGLProgramState(GLProgramState::getOrCreateWithGLProgramName(GLProgram::SHADER_NAME_POSITION_COLOR));
        if (_points.size() > 2) {
            Color4F brushColor {0, 0, 0, 1};
            if (_enableLineSmoothing) {
                auto smoothPoints = smoothLinePoints(_points);
                drawLines(renderer, transform, smoothPoints, brushColor);
            }
            else {
                drawLines(renderer, transform, _points, brushColor);
            }
            _points.erase(_points.begin(), _points.end() - 2);
        }
        
        _renderTexture->end();
        
        Node::draw(renderer, transform, flags);
    }

    void drawLines(Renderer *renderer, const Mat4 &transform, std::vector<LinePoint> &linePoints, Color4F color)
    {
        const Color4F fadeOutColor {0, 0, 0, 0};
        
        Vec2 prevPoint = linePoints[0].pos;
        float prevValue = linePoints[0].width;
        
        _vertices.clear();
        _indices.clear();
        
        for (int i = 1; i < linePoints.size(); i++) {
            auto curPoint = linePoints[i].pos;
            float curValue = linePoints[i].width;
            
            if (curPoint.fuzzyEquals(prevPoint, 0.0001f)) {
                continue;
            }
            
            Vec2 dir = curPoint - prevPoint;
            Vec2 perp = dir.getPerp().getNormalized();
            Vec2 A = prevPoint + perp * prevValue / 2;
            Vec2 B = prevPoint - perp * prevValue / 2;
            Vec2 C = curPoint + perp * curValue / 2;
            Vec2 D = curPoint - perp * curValue / 2;
            
            if (_connectingLine || _indices.size() > 0) {
                A = _prevC;
                B = _prevD;
            } else if (_vertices.size() == 0) {
//                [circlesPoints addObject:pointValue];
//                [circlesPoints addObject:[linePoints objectAtIndex:i - 1]];
            }
            
            triangulateRect(A, B, C, D, color, _vertices, _indices);
            
            _prevD = D;
            _prevC = C;
            if (_finishingLine && (i == linePoints.size() - 1)) {
//                [circlesPoints addObject:[linePoints objectAtIndex:i - 1]];
//                [circlesPoints addObject:pointValue];
                _finishingLine = false;
            }
            
            prevPoint = curPoint;
            prevValue = curValue;
            
            //! Add overdraw
            Vec2 F = A + perp * _overdraw;
            Vec2 G = C + perp * _overdraw;
            Vec2 H = B - perp * _overdraw;
            Vec2 I = D - perp * _overdraw;
            
            if (_connectingLine || _indices.size() > 6) {
                F = _prevG;
                H = _prevI;
            }
            _prevG = G;
            _prevI = I;
            
            triangulateRect(F, fadeOutColor, A, color, G, fadeOutColor, C, color, _vertices, _indices, 0);
            triangulateRect(B, color, H, fadeOutColor, D, color, I, fadeOutColor, _vertices, _indices, 0);
        }
        
        TrianglesCommand::Triangles trs{_vertices.data(), _indices.data(), static_cast<ssize_t>(_vertices.size()), static_cast<ssize_t>(_indices.size())};
        _triangleCommand.init(getGlobalZOrder(), 0, getGLProgramState(), cocos2d::BlendFunc::ALPHA_PREMULTIPLIED, trs, transform, 0);
        renderer->addCommand(&_triangleCommand);
        
        if (_indices.size() > 0) {
            _connectingLine = true;
        }
        
    }
    
    static std::vector<LinePoint> smoothLinePoints(std::vector<LinePoint> &linePoints)
    {
        std::vector<LinePoint> result;
        
        if (linePoints.size() > 2) {
            for (unsigned int i = 2; i < linePoints.size(); ++i) {
                auto prev2 = linePoints[i - 2];
                auto prev1 = linePoints[i - 1];
                auto cur = linePoints[i];
                
                Vec2 midPoint1 = (prev1.pos + prev2.pos) * .5;
                Vec2 midPoint2 = (cur.pos + prev1.pos) * .5;
                
                int segmentDistance = 2;
                float distance = (midPoint1 - midPoint2).getLength();
                int numberOfSegments = MIN(128, MAX(floorf(distance / segmentDistance), 32));
                
                float t = 0.0f;
                float step = 1.0f / numberOfSegments;
                for (int j = 0; j < numberOfSegments; j++) {
                    LinePoint newPoint;
                    newPoint.pos = midPoint1 * powf(1 - t, 2) + prev1.pos * 2.0f * (1 - t) * t + midPoint2 * t * t;
                    newPoint.width = powf(1 - t, 2) * ((prev1.width + prev2.width) * 0.5f) + 2.0f * (1 - t) * t * prev1.width + t * t * ((cur.width + prev1.width) * 0.5f);
                    
                    result.push_back(newPoint);
                    t += step;
                }
                LinePoint finalPoint;
                finalPoint.pos = midPoint2;
                finalPoint.width = (cur.width + prev1.width) * 0.5f;
                result.push_back(finalPoint);
            }

        }
        
        return result;
    }
    
    bool onTouchBegan(cocos2d::Touch *touch, cocos2d::Event *unused_event)
    {
        Vec2 location = touch->getLocation();
//        CCLOG("touch began: %.2f %.2f", location.x, location.y);
        
        _points.clear();
        
        float size = extractSize();
        startNewLine(location, size);
        addPoint(location, size);
        addPoint(location, size);
        
        return true;
    }
    
    void onTouchMoved(Touch *touch, Event *event)
    {
        Vec2 location = touch->getLocation();
//        CCLOG("touch moved: %.2f %.2f", location.x, location.y);
        
        //! skip points that are too close
        float eps = 1.5f;
        if (_points.size() > 0) {
            auto v = _points.back().pos - location;
            float length = v.getLength();
            
            if (length < eps) {
                return;
            } else {
            }
        }
        
        float size = extractSize();
        addPoint(location, size);
    }
    
    void onTouchEnded(cocos2d::Touch *touch, cocos2d::Event *unused_event)
    {
        Vec2 location = touch->getLocation();
//        CCLOG("touch ended: %.2f %.2f", location.x, location.y);
//        CCLOG("line has points %lu", _points.size());
    }
    
    void onTouchCancelled(cocos2d::Touch *touch, cocos2d::Event *unused_event)
    {
        Vec2 location = touch->getLocation();
        float size = extractSize();
        endLine(location, size);
    }
    
private:
    std::vector<LinePoint> _points;
    bool _connectingLine, _finishingLine;
    Vec2 _prevC, _prevD, _prevG, _prevI;
    float _overdraw;
    bool _enableLineSmoothing;
    
    TrianglesCommand _triangleCommand;
    CustomCommand _customCommand;
    
    std::vector<V3F_C4B_T2F> _vertices;
    std::vector<unsigned short> _indices;
    
    RenderTexture *_renderTexture;

};

#endif /* LineDrawer_hpp */
