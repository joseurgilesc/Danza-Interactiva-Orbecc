#include "scenes/BodyContourPaintScene.h"
#include "registry/ParameterRegistry.h"
#include "mapping/ParameterTypes.h"
#include "ofMain.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace modular {

void BodyContourPaintScene::setupPalettes() {
    if(!palettes_.empty())return;
    palettes_={
        {{1,0,1},{1,.08f,.58f},{1,0,.5f},{1,.41f,.71f},{1,0,.8f}},
        {{0,1,1},{0,.81f,.82f},{.25f,.88f,.82f},{0,.75f,1},{.02f,.85f,1}},
        {{0,1,0},{.22f,1,.08f},{.2f,.8f,.2f},{0,.98f,.6f},{.49f,.99f,0}},
        {{1,.42f,0},{1,.27f,0},{1,.55f,0},{1,.5f,.31f},{1,.39f,.28f}},
        {{1,1,0},{1,.84f,0},{.8f,1,0},{1,1,.2f},{.9f,1,0}},
    };
}
ofFloatColor BodyContourPaintScene::getColor(int pi,float var) {
    auto& p=palettes_[pi%palettes_.size()];
    auto c=p[(int)ofRandom(p.size())];
    c+={var*ofRandom(-.12f,.12f),var*ofRandom(-.12f,.12f),var*ofRandom(-.12f,.12f)};
    c.a=.85f+var*.15f; return c;
}

BodyContourPaintScene::BodyContourPaintScene(){initParams();}
BodyContourPaintScene::~BodyContourPaintScene(){unregisterParams();}

void BodyContourPaintScene::initParams() {
    if(!params_.empty())return;
    auto add=[&](auto& k,auto& l,float lo,float hi,float d,float st,auto& desc,ParameterType t=ParameterType::FLOAT){
        auto p=std::make_shared<Parameter>();
        p->id=getId()+"."+k;p->label=l;p->type=t;p->scope=ParameterScope::SCENE;
        p->min=lo;p->max=hi;p->defaultValue=d;p->currentValue=d;
        p->step=st;p->group=getId();p->description=desc;p->hidden=false;p->writable=true;
        params_[k]=p;
    };
    // PinturaFluorescente orbit
    add("k","k (pull)",.01f,.5f,k_,.01f,"Center attraction");
    add("A","A (wave)",0,50,A_,1,"Wave amplitude px");
    add("w","w (wave f)",.01f,2,w_,.01f,"Wave frequency");
    add("q","q (wave sp)",0,.01f,q_,.0005f,"Wave spatial");
    add("beta","beta",.1f,.99f,beta_,.01f,"Damping");
    add("S","S (swirl)",0,400,S_,10,"Swirl amplitude px");
    add("ws","ws (swirl f)",.01f,2,ws_,.01f,"Swirl frequency");
    add("qs","qs (swirl sp)",0,.01f,qs_,.0005f,"Swirl spatial");
    // ring
    add("ringPull","Ring Pull",.1f,3,ringPull_,.1f,"Attraction to ring");
    add("repelForce","Repel",0,1,repelForce_,.05f,"Expansion repulsion");
    // spawn
    add("spreadRadius","Spread",30,300,spreadRadius_,10,"Spawn scatter");
    add("spawnRate","Spawn Rate",5,120,spawnRate_,5,"Particles/s");
    // visual
    add("particleLife","Life",1,10,particleLife_,.5f,"Seconds");
    add("particleSize","Size",1,12,particleSize_,.5f,"Radius px");
    // Indexed size params for _array MIDI mapping (8 groups)
    for (int gi = 0; gi < 8; gi++) {
        auto p = std::make_shared<Parameter>();
        p->id = getId() + ".particleSize_" + std::to_string(gi);
        p->label = "Size G" + std::to_string(gi);
        p->type = ParameterType::FLOAT;
        p->scope = ParameterScope::SCENE;
        p->min = 1; p->max = 12;
        p->defaultValue = particleSize_; p->currentValue = particleSize_;
        p->step = 0.5f; p->group = getId();
        p->description = "Particle size group " + std::to_string(gi);
        p->hidden = false; p->writable = true;
        params_["particleSize_" + std::to_string(gi)] = p;
    }
    add("gravity","Gravity",0,.1f,gravity_,.002f,"Downward");
    add("maxParticles","Max",BCP_MIN_MAX,BCP_MAX_MAX,(float)maxParticles_,100,"Pool",ParameterType::INT);
}
void BodyContourPaintScene::registerParams(){
    if(paramsReg_)return;initParams();
    auto& r=ParameterRegistry::getGlobal();
    for(auto& e:params_)if(e.second)r.registerParam(*e.second);
    paramsReg_=true;
}
void BodyContourPaintScene::unregisterParams(){
    if(!paramsReg_)return;
    auto& r=ParameterRegistry::getGlobal();
    for(auto& e:params_)if(e.second)r.unregisterParam(e.second->id);
    paramsReg_=false;
}
void BodyContourPaintScene::syncParams(){
    auto rf=[&](auto& k,float& t){auto it=params_.find(k);if(it!=params_.end()&&it->second)t=it->second->currentValue;};
    auto ri=[&](auto& k,int& t){auto it=params_.find(k);if(it!=params_.end()&&it->second)t=(int)it->second->currentValue;};
    rf("k",k_);rf("A",A_);rf("w",w_);rf("q",q_);rf("beta",beta_);
    rf("S",S_);rf("ws",ws_);rf("qs",qs_);
    rf("ringPull",ringPull_);rf("repelForce",repelForce_);
    rf("spreadRadius",spreadRadius_);rf("spawnRate",spawnRate_);
    rf("particleLife",particleLife_);rf("particleSize",particleSize_);
    rf("gravity",gravity_);ri("maxParticles",maxParticles_);
    for (int gi = 0; gi < 8; gi++) {
        std::string key = "particleSize_" + std::to_string(gi);
        rf(key, groupSizes_[gi]);
    }
}
void BodyContourPaintScene::applyResize(){
    int t=glm::clamp(maxParticles_,BCP_MIN_MAX,BCP_MAX_MAX);
    if((int)particles_.size()==t)return;
    size_t old=particles_.size();particles_.resize(t);
    int c=0;for(size_t i=0;i<old;i++)if(particles_[i].active)c++;
    activeCount_=c;
}

void BodyContourPaintScene::setup(){
    ofEnableAlphaBlending();setupPalettes();initParams();ofSetBackgroundAuto(true);
    float bg=10;auto& r=ParameterRegistry::getGlobal();
    if(auto p=r.getParam("global.backgroundColor"))bg=p->currentValue;
    ofBackground((int)bg);
}
void BodyContourPaintScene::activate(){
    isActive_=true;isPaused_=false;
    ofSetWindowTitle("Body Contour Paint");ofSetFrameRate(60);
    float bg=10;auto& r=ParameterRegistry::getGlobal();
    if(auto p=r.getParam("global.backgroundColor"))bg=p->currentValue;ofBackground((int)bg);
    if(auto pp=r.getParam("global.currentPalette"))currentPalette_=(int)pp->currentValue%5;
    registerParams();
    auto px=getId()+".";
    for(auto& e:params_){auto f=px+e.first;if(auto p=r.getParam(f))setParameter(e.first,p->currentValue);}
    syncParams();
    particles_.clear();particles_.resize(glm::clamp(maxParticles_,BCP_MIN_MAX,BCP_MAX_MAX));
    activeCount_=0;
    circleCenter_={ofGetWidth()/2.f,ofGetHeight()/2.f};circleRadius_=prevRadius_=100;
    spawnAccum_=0;
}
void BodyContourPaintScene::deactivate(){
    isActive_=false;unregisterParams();
    interactionPoints_.clear();particles_.clear();activeCount_=0;
}
void BodyContourPaintScene::pause(){isPaused_=true;}
void BodyContourPaintScene::resume(){isPaused_=false;}

void BodyContourPaintScene::buildCircle(){
    prevRadius_=circleRadius_;
    std::vector<glm::vec2> pts;
    for(auto& e:interactionPoints_)if(e.second.active)pts.push_back(e.second.position);
    float targetRadius=circleRadius_;
    if(pts.size()>=3){
        circleCenter_={0,0};
        for(auto& p:pts)circleCenter_+=p;
        circleCenter_/=(float)pts.size();
        targetRadius=0;
        for(auto& p:pts)targetRadius+=glm::distance(p,circleCenter_);
        targetRadius/=(float)pts.size();
        targetRadius=std::max(50.f,targetRadius);
    }else if(pts.size()==1){
        circleCenter_=pts[0];targetRadius=50;
    }else{
        circleCenter_={ofGetWidth()/2.f,ofGetHeight()/2.f};targetRadius=100;
    }
    // Smooth transition — 30% toward target per frame
    circleRadius_=glm::mix(prevRadius_,targetRadius,.3f);
}

void BodyContourPaintScene::spawnParticles(float dt){
    int cap=(int)particles_.size();if(activeCount_>=cap)return;
    spawnAccum_+=spawnRate_*dt;
    int n=std::min((int)spawnAccum_,cap-activeCount_);if(n<=0)return;
    spawnAccum_-=(float)n;

    for(int s=0;s<n;s++){
        int slot=-1;
        for(size_t i=0;i<particles_.size();i++)if(!particles_[i].active){slot=(int)i;break;}
        if(slot<0)break;

        float a=ofRandom(0,glm::two_pi<float>());
        glm::vec2 ringPos=circleCenter_+glm::vec2(std::cos(a),std::sin(a))*circleRadius_;
        float d=spreadRadius_*ofRandom(.1f,1.f);
        glm::vec2 offset(std::cos(a+ofRandom(-.3f,.3f)),std::sin(a+ofRandom(-.3f,.3f)));

        auto& p=particles_[slot];
        p.active=true;
        p.pos=ringPos+offset*d;
        glm::vec2 tangent(-std::sin(a),std::cos(a));
        p.vel=tangent*S_*ofRandom(.3f,1.f);
        p.maxLife=particleLife_*ofRandom(.7f,1.4f);p.life=p.maxLife;
        p.size=groupSizes_[slot%8]*ofRandom(.15f,2.5f);
        p.phase=ofRandom(0,glm::two_pi<float>());
        p.color=getColor(currentPalette_,.8f);
        activeCount_++;
    }
}

// ── PinturaFluorescente physics applied to ring projection ────────────────

void BodyContourPaintScene::updateParticles(float dt){
    float t=ofGetElapsedTimef();

    // Gentle expansion repulsion — clamped to avoid chaos
    float rawDRadius=(circleRadius_-prevRadius_)/std::max(dt,.001f);
    float expandForce=glm::clamp(rawDRadius*repelForce_,-200.f,200.f);

    for(auto& p:particles_){
        if(!p.active)continue;
        p.life-=dt;
        if(p.life<=0){p.active=false;activeCount_--;continue;}

        // Project onto ring → this is the "center" for orbital physics
        glm::vec2 toCenter=p.pos-circleCenter_;
        float dist=glm::length(toCenter)+.0001f;
        glm::vec2 radial=toCenter/dist;
        glm::vec2 ringPoint=circleCenter_+radial*circleRadius_;

        // Vector from ring projection to particle
        glm::vec2 r=p.pos-ringPoint;
        float d=glm::length(r)+.0001f;
        glm::vec2 n=r/d;
        glm::vec2 tangent(-n.y,n.x);

        // ── PinturaFluorescente orbital forces ──
        glm::vec2 Fc=-k_*r;                                    // center attraction
        float wa=w_*t-q_*d+p.phase;
        glm::vec2 Fw=A_*std::sin(wa)*n;                        // radial wave
        float sa=ws_*t-qs_*d+p.phase;
        glm::vec2 Fs=S_*std::cos(sa)*tangent;                  // tangential swirl
        glm::vec2 Fd=-beta_*p.vel;                              // damping

        // Ring pull: strong radial attraction to keep on circle
        glm::vec2 Fr=radial*(circleRadius_-dist)*ringPull_;

        // Expansion repulsion: push outward when circle grows
        glm::vec2 Fe=radial*expandForce;

        // Gravity
        glm::vec2 Fg={0,gravity_};

        p.vel+=(Fc+Fw+Fs+Fd+Fr+Fe+Fg)*dt;
        p.pos+=p.vel*dt;

        float sp=glm::length(p.vel);
        if(sp>BCP_MAX_SPEED)p.vel=(p.vel/sp)*BCP_MAX_SPEED;
    }
}

void BodyContourPaintScene::update(float dt){
    if(!isActive_||isPaused_)return;
    dt=glm::clamp(dt,0.f,.1f);
    for(auto& e:interactionPoints_)e.second.update(dt);
    int cap=glm::clamp(maxParticles_,BCP_MIN_MAX,BCP_MAX_MAX);
    if((int)particles_.size()!=cap)applyResize();
    buildCircle();
    spawnParticles(dt);
    updateParticles(dt);
}

// ── PinturaFluorescente-style drawing ─────────────────────────────────────

void BodyContourPaintScene::drawRing(){
    if(circleRadius_<10)return;
    ofEnableAlphaBlending();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofNoFill();
    ofSetColor(255,200,150,30);ofSetLineWidth(5);
    ofDrawCircle(circleCenter_.x,circleCenter_.y,circleRadius_);
    ofSetColor(255,220,180,60);ofSetLineWidth(2);
    ofDrawCircle(circleCenter_.x,circleCenter_.y,circleRadius_);
    ofSetColor(255,240,210,100);ofSetLineWidth(1);
    ofDrawCircle(circleCenter_.x,circleCenter_.y,circleRadius_);
    ofDisableBlendMode();
}

void BodyContourPaintScene::drawParticles(){
    ofEnableAlphaBlending();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofFill();  // PinturaFluorescente style: filled circles
    // Depth relation: body proximity scales particle size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);
    for(auto& p:particles_){
        if(!p.active)continue;
        float ratio=p.life/p.maxLife;
        float alpha=ratio*180;
        // Outer soft glow
        ofSetColor(p.color.r*255,p.color.g*255,p.color.b*255,alpha*.35f);
        ofDrawCircle(p.pos.x,p.pos.y,p.size*2.2f*depthScale);
        // Mid glow
        ofSetColor(p.color.r*255,p.color.g*255,p.color.b*255,alpha*.6f);
        ofDrawCircle(p.pos.x,p.pos.y,p.size*1.3f*depthScale);
        // Core
        ofSetColor(p.color.r*255,p.color.g*255,p.color.b*255,alpha);
        ofDrawCircle(p.pos.x,p.pos.y,p.size*.6f*depthScale);
    }
    ofDisableBlendMode();
}

void BodyContourPaintScene::draw(){
    if(!isActive_)return;
    ofPushStyle();
    drawParticles();
    if(debug_){
        ofSetColor(255,255,0,180);ofDrawCircle(circleCenter_.x,circleCenter_.y,5);
        ofSetColor(0,0,0,210);ofDrawRectangle(5,ofGetHeight()-28,320,24);
        ofSetColor(0,255,0,250);
        auto s="BodyContourPaint | P:"+std::to_string(activeCount_)+"/"+std::to_string(particles_.size())
              +" | R:"+std::to_string((int)circleRadius_);
        ofDrawBitmapString(s,10,ofGetHeight()-9);
    }
    ofPopStyle();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string BodyContourPaintScene::getId()const{return"bodyContourPaint";}
std::string BodyContourPaintScene::getName()const{return"Body Contour Paint";}
std::vector<std::string> BodyContourPaintScene::getParameterIds()const{
    std::vector<std::string>ids;for(auto&e:params_)ids.push_back(e.first);return ids;
}
std::vector<std::string> BodyContourPaintScene::getInteractionPointIds()const{
    std::vector<std::string>ids;for(auto&e:interactionPoints_)ids.push_back(e.first);return ids;
}
void BodyContourPaintScene::clearInteractionPoints(){interactionPoints_.clear();}
std::shared_ptr<Parameter> BodyContourPaintScene::getParameter(const std::string&id){
    auto it=params_.find(id);return it!=params_.end()?it->second:nullptr;
}
bool BodyContourPaintScene::setParameter(const std::string&id,float v){
    auto key=id;auto pfx=getId()+".";if(key.find(pfx)==0)key=key.substr(pfx.size());
    auto it=params_.find(key);if(it==params_.end()||!it->second)return false;
    it->second->currentValue=v;syncParams();return true;
}
bool BodyContourPaintScene::setParameter(const std::string&id,const glm::vec2&v){
    if(!isActive_)return true;
    auto& pt=interactionPoints_[id];pt.sourceName=id;pt.decayRate=.08f;
    pt.setTarget(v,1.f,ofGetWidth(),ofGetHeight());return true;
}
bool BodyContourPaintScene::setParameter(const std::string& paramId,const glm::vec3& value){
    if(!isActive_)return true;
    auto& pt=interactionPoints_[paramId];pt.sourceName=paramId;pt.decayRate=.08f;
    pt.setTarget(glm::vec2(value.x,value.y),1.f,ofGetWidth(),ofGetHeight(),value.z);return true;
}
std::vector<SuggestedMapping> BodyContourPaintScene::getSuggestedMappings()const{return{};}
Preset BodyContourPaintScene::getDefaultPreset()const{
    Preset pr;pr.id="default";pr.name="Default";pr.sceneId=getId();pr.isDefault=true;
    for(auto&e:params_){Parameter cp=*e.second;cp.currentValue=cp.defaultValue;pr.parameters.push_back(cp);}
    return pr;
}
SceneMetadata BodyContourPaintScene::getSceneMetadata()const{
    SceneMetadata m;m.id=getId();m.name=getName();
    m.description="PinturaFluorescente orbital physics on a ring perimeter. Joints build a circle; particles flow+orbit along it.";
    m.author="Proyecto Danza";m.version="1.0.0";
    m.tags={"ring","orbit","wave","swirl","paint","pintura"};m.category="visual";
    m.hasInternalState=true;m.isTimeBased=false;m.estimatedDuration=0;
    return m;
}
void BodyContourPaintScene::onBeat(int){}void BodyContourPaintScene::onBar(int){}
bool BodyContourPaintScene::isValid()const{return isValid_;}
std::string BodyContourPaintScene::getInvalidReason()const{return invalidReason_;}
void BodyContourPaintScene::unloadResources(){particles_.clear();activeCount_=0;}
void BodyContourPaintScene::reloadResources(){setup();}
void BodyContourPaintScene::keyPressed(int){}void BodyContourPaintScene::keyReleased(int){}
void BodyContourPaintScene::mousePressed(int,int,int){}void BodyContourPaintScene::mouseReleased(int,int,int){}
void BodyContourPaintScene::mouseDragged(int,int,int){}void BodyContourPaintScene::mouseMoved(int,int){}
void BodyContourPaintScene::windowResized(int,int){}

ScenePtr CreateBodyContourPaintScene(){return std::make_shared<BodyContourPaintScene>();}

} // namespace modular
