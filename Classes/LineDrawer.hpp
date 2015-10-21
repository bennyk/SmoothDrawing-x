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
#include "GestureRecognizers.hpp"

using namespace cocos2d;

class LineDrawer : public Node {
    
public:
    static constexpr float DefaultLineWidth = 1.0f;
    static constexpr float Overdraw = .5f;
    static const Color4F BackgroundColor;
    
    struct LinePoint {
        Vec2 pos;
        float width;
        
        LinePoint (Vec2 p, float w) : pos(p), width(w) {}
        LinePoint() : pos {0, 0}, width {DefaultLineWidth} {}
    };
    
    struct CirclePoint {
        Vec2 pos;
        float width;
        Vec2 dir;
        CirclePoint (Vec2 p, float w, Vec2 d) : pos(p), width(w), dir(d) {}
    };

    
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
    
    LineDrawer () : _enableLineSmoothing(true), _lastSize(0.0) {}
    ~LineDrawer() {
        if (_renderTexture != nullptr)
            _renderTexture->release();
        
        if (_panGestureRecognizer != nullptr)
            _panGestureRecognizer->release();
        
        if (_longPressGestureRecognizer != nullptr)
            _longPressGestureRecognizer->release();
    }
    
    virtual bool init()
    {
        _panGestureRecognizer = PanGestureRecognizer::create();
        _panGestureRecognizer->retain();
        
        _panGestureRecognizer->setTarget(CC_CALLBACK_1(LineDrawer::handlePanGestureRecognizer, this));
        _panGestureRecognizer->addWithSceneGraphPriority(this->getEventDispatcher(), this);
        
        _longPressGestureRecognizer = LongPressGestureRecognizer::create();
        _longPressGestureRecognizer->retain();
        
        _longPressGestureRecognizer->setTarget(CC_CALLBACK_1(LineDrawer::handleLongPressGestureRecognizer, this));
        _longPressGestureRecognizer->addWithSceneGraphPriority(this->getEventDispatcher(), this);
        
        Size size = Director::getInstance()->getWinSize();
        _renderTexture = RenderTexture::create(size.width, size.height, Texture2D::PixelFormat::RGBA8888);
        _renderTexture->retain();
        
        _renderTexture->clear(BackgroundColor.r, BackgroundColor.g, BackgroundColor.b, BackgroundColor.a);
        _renderTexture->setAnchorPoint(Vec2 {0, 0});
        _renderTexture->setPosition(Vec2 {size.width * .5f, size.height * .5f});
        this->addChild(_renderTexture);
        
        return true;
    }
    
    void handleLongPressGestureRecognizer(BasicGestureRecognizer *r)
    {
//        LongPressGestureRecognizer *recognizer = static_cast<LongPressGestureRecognizer *>(r);
//        CCLOG("got long press");
        _renderTexture->beginWithClear(BackgroundColor.r, BackgroundColor.g, BackgroundColor.b, BackgroundColor.a);
        _renderTexture->end();
    }
    
    void handlePanGestureRecognizer(BasicGestureRecognizer *r)
    {
//        CCLOG("received gesture %d", recognizer->getState());
        PanGestureRecognizer *recognizer = static_cast<PanGestureRecognizer *>(r);
        
        switch (recognizer->getState()) {
            case PanGestureRecognizer::Began: {
                Vec2 location = recognizer->getLocation();
                //        CCLOG("touch began: %.2f %.2f", location.x, location.y);

                _points.clear();
                
                _lastSize = 0.0;
                float size = extractSize(recognizer->getVelocity());
                
                startNewLine(location, size);
                addPoint(location, size);
                addPoint(location, size);

                break;
            }
                
            case PanGestureRecognizer::Changed: {
                Vec2 location = recognizer->getLocation();
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
                
                float size = extractSize(recognizer->getVelocity());
                addPoint(location, size);
                break;
            }
                
            case PanGestureRecognizer::Completed: {
                Vec2 location = recognizer->getLocation();
                float size = extractSize(recognizer->getVelocity());
                endLine(location, size);
                //        CCLOG("touch ended: %.2f %.2f", location.x, location.y);
                //        CCLOG("line has points %lu", _points.size());
                
                break;
            }
                
            default:
                break;
        }
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
    
    float extractSize(Vec2 velocity)
    {
        float vel = velocity.getLength();
        float size = vel / 166.0f;
        size = clampf(size, 1, 40);
        
        if (_lastSize != 0.0) {
            size = size * 0.8f + _lastSize * 0.2f;
        }
        _lastSize = size;
        
//        CCLOG("extracted size %.2f", size);

        return size;
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
    
    static void triangulateCircle(CirclePoint circle, Color4F color, float overdraw, std::vector<V3F_C4B_T2F> &vertices, std::vector<unsigned short> &indices, float z = 0)
    {
        Color4F fadeOutColor = Color4F {};
//        Color4F fadeOutColor = Color4F {1, 0, 0, .5};
        
        int numberOfSegments = 32;
        float anglePerSegment = (float)(M_PI / (numberOfSegments - 1));
        
        //! we need to cover M_PI from this, dot product of normalized vectors is equal to cos angle between them... and if you include rightVec dot you get to know the correct direction :)
        Vec2 perp = circle.dir.getPerp();
        
        float angle = acosf(perp.dot(Vec2 {0, 1}));
        const float rightDot = perp.dot(Vec2 {1, 0});
        if (rightDot < 0.0f) {
            angle *= -1;
        }
        
        const float radius = circle.width * .5;
        const unsigned short centerIndex = vertices.size();
        
        vertices.push_back(V3F_C4B_T2F {Vec3 {circle.pos.x, circle.pos.y, z}, Color4B {color}, Tex2F {}});
        
        int prevIndex;
        Vec2 prevPoint, prevDir;
        for (unsigned int i = 0; i < numberOfSegments; ++i) {
            Vec2 dir = Vec2 {sinf(angle), cosf(angle)};
            Vec2 curPoint = Vec2 {circle.pos.x + radius * dir.x, circle.pos.y + radius * dir.y};
            
            int currentIndex = vertices.size();
            vertices.push_back(V3F_C4B_T2F {Vec3 {curPoint.x, curPoint.y, z}, Color4B {color}, Tex2F {}});
            
            if (i > 0) {
                indices.push_back(centerIndex);
                indices.push_back(prevIndex);
                indices.push_back(currentIndex);
                
//                CCLOG("circle %d %lu %lu", centerIndex, vertices.size() - 2, vertices.size() - 1);
                
                // triangulate a overdrawn rect
                Vec2 prevOverdrawnPoint = prevPoint + prevDir * overdraw;
                Vec2 currentOverdrawnPoint = curPoint + dir * overdraw;
                
                auto prevOverdrawIndex = vertices.size();
                vertices.push_back(V3F_C4B_T2F {Vec3 {prevOverdrawnPoint.x, prevOverdrawnPoint.y, z}, Color4B {fadeOutColor}, Tex2F {}});
                
                auto curOverdrawIndex = vertices.size();
                vertices.push_back(V3F_C4B_T2F {Vec3 {currentOverdrawnPoint.x, currentOverdrawnPoint.y, z}, Color4B {fadeOutColor}, Tex2F {}});
                
                indices.push_back(prevIndex);
                indices.push_back(curOverdrawIndex);
                indices.push_back(prevOverdrawIndex);
                
                indices.push_back(prevIndex);
                indices.push_back(currentIndex);
                indices.push_back(curOverdrawIndex);
            }
            
            
            prevIndex = currentIndex;
            prevPoint = curPoint;
            prevDir = dir;
            angle += anglePerSegment;
        }

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
        
        LinePoint &prevPoint = linePoints[0];
        
        _vertices.clear();
        _indices.clear();
        
        std::vector<CirclePoint> circles;
        
        for (int i = 1; i < linePoints.size(); i++) {
            auto curPoint = linePoints[i];
            
            if (curPoint.pos.fuzzyEquals(prevPoint.pos, 0.0001f)) {
                continue;
            }
            
            Vec2 dir = curPoint.pos - prevPoint.pos;
            Vec2 perp = dir.getPerp().getNormalized();
            Vec2 A = prevPoint.pos + perp * prevPoint.width / 2;
            Vec2 B = prevPoint.pos - perp * prevPoint.width / 2;
            Vec2 C = curPoint.pos + perp * curPoint.width / 2;
            Vec2 D = curPoint.pos - perp * curPoint.width / 2;
            
            if (_connectingLine || _indices.size() > 0) {
                A = _prevC;
                B = _prevD;
            } else if (_indices.size() == 0) {
                circles.push_back(CirclePoint {curPoint.pos, curPoint.width, (linePoints[i - 1].pos - curPoint.pos).getNormalized()});
            }
            
            triangulateRect(A, B, C, D, color, _vertices, _indices);
            
            _prevD = D;
            _prevC = C;
            if (_finishingLine && (i == linePoints.size() - 1)) {
                circles.push_back(CirclePoint {curPoint.pos, curPoint.width, (curPoint.pos - linePoints[i - 1].pos).getNormalized()});
                _finishingLine = false;
            }
            
            prevPoint = curPoint;
            
            //! Add overdraw
            Vec2 F = A + perp * Overdraw;
            Vec2 G = C + perp * Overdraw;
            Vec2 H = B - perp * Overdraw;
            Vec2 I = D - perp * Overdraw;
            
            if (_connectingLine || _indices.size() > 6) {
                F = _prevG;
                H = _prevI;
            }
            _prevG = G;
            _prevI = I;
            
            triangulateRect(F, fadeOutColor, A, color, G, fadeOutColor, C, color, _vertices, _indices, 0);
            triangulateRect(B, color, H, fadeOutColor, D, color, I, fadeOutColor, _vertices, _indices, 0);
        }
        
        for (auto c : circles) {
            triangulateCircle(c, color, Overdraw, _vertices, _indices);
        }
        
        TrianglesCommand::Triangles trs{&_vertices[0], &_indices[0], static_cast<ssize_t>(_vertices.size()), static_cast<ssize_t>(_indices.size())};
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
    
private:
    std::vector<LinePoint> _points;
    bool _connectingLine, _finishingLine;
    Vec2 _prevC, _prevD, _prevG, _prevI;
    bool _enableLineSmoothing;
    
    PanGestureRecognizer *_panGestureRecognizer;
    LongPressGestureRecognizer *_longPressGestureRecognizer;
    
    TrianglesCommand _triangleCommand;
    
    std::vector<V3F_C4B_T2F> _vertices;
    std::vector<unsigned short> _indices;
    
    RenderTexture *_renderTexture;
    float _lastSize;

};

#endif /* LineDrawer_hpp */
