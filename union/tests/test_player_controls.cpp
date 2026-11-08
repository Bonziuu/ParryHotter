// Adapter contract simulation. The lightweight engine doubles below test
// input ownership and native-call sequencing; they do not validate the SDK ABI.
#include <array>
#include <map>
#include <cmath>
#define KEY_W 17
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32
#define KEY_UP 200
#define KEY_DOWN 208
#define KEY_LEFT 203
#define KEY_RIGHT 205
#define KEY_SPACE 57
#define KEY_F11 87
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define GAME_UP 3
#define GAME_DOWN 4
#define GAME_LEFT 1
#define GAME_RIGHT 2
#define GAME_ACTION 5
#define GAME_STRAFELEFT 18
#define GAME_STRAFERIGHT 19
#define GAME_SMOVE 11
#define GAME_SMOVE2 12
#define MOUSE_BUTTONLEFT 522
#define MOUSE_BUTTONRIGHT 523
#define KEY_LCONTROL 29
#define KEY_LMENU 56
#include <vector>
namespace Gothic_I_Classic {
struct zVEC3 { float a[3]; zVEC3(float x=0,float y=0,float z=0):a{x,y,z}{} float& operator[](int n){return a[n];} float operator[](int n)const{return a[n];} };
struct zCVob { zVEC3 at{0,0,1},position; int begin=0,end=0; float biggestStep=0; zVEC3 GetAtVectorWorld(){return at;} zVEC3 GetPositionWorld(){return position;} void SetPositionWorld(zVEC3 p){position=p;} void BeginMovement(){++begin;} void EndMovement(){++end;} void MoveWorld(float x,float y,float z){position[0]+=x;position[1]+=y;position[2]+=z;biggestStep=std::max(biggestStep,std::hypot(x,z));} void SetHeadingYWorld(zVEC3 p){at=p;} void RotateWorldY(float d){float angle=std::atan2(at[0],at[2])+d*0.01745329252f;at=zVEC3(std::sin(angle),0,std::cos(angle));} };
struct zCModelAni {};
struct zCModel { zVEC3 aniTransScale{1,1,1}; zCModelAni ani; bool active=false; zCModelAni* GetAniFromAniID(int){return &ani;} int IsAniActive(zCModelAni*){return active;} };
struct zCRigidBody { zVEC3 v; void SetVelocity(zVEC3 p){v=p;} };
struct oCNpc: zCVob { bool physicsEnabled=true; zCRigidBody bodyPhysics; zCRigidBody* GetRigidBody(){return &bodyPhysics;} bool openinventory=false,dead=false,human=true,movLock=false; int body=0; zCModel mdl; bool IsDead(){return dead;} bool IsHuman(){return human;} bool IsMovLock(){return movLock;} int GetBodyState(){return body;} zCModel* GetModel(){return &mdl;} };
struct oCGame { bool inLoadSaveGame=false,inLevelChange=false,paused=false; bool IsOnPause(){return paused;} };
template<class T> struct zCArray { std::vector<T> v; int GetNumInList() const {return int(v.size());} T& operator[](int i){return v[i];} const T& operator[](int i) const {return v[i];} void InsertEnd(const T& x){v.push_back(x);} };
struct zCInput_Win32 { std::map<int,std::vector<unsigned short>> bindings; int bindCalls=0;
 int IsBinded(unsigned short a,unsigned short k){for(auto x:bindings[a]) if(x==k) return 1; return 0;}
 int GetBinding(unsigned short a,zCArray<unsigned short>& out){out.v=bindings[a];return 1;}
 void Bind(unsigned short a,const zCArray<unsigned short>& in){bindings[a]=in.v;++bindCalls;}
 std::map<int,int> keys,toggled,states; bool left=false,right=false; int KeyPressed(int n){return keys[n];} int KeyToggled(int n){int r=toggled[n];toggled[n]=0;return r;} int GetMouseButtonPressedRight(){return right;} int GetMouseButtonPressedLeft(){return left;} void GetMousePos(float&x,float&y,float&z){x=0;y=0;z=0;} int SetState(unsigned short n,int v){states[n]=v;return v;} };
struct zCAICamera { static zCAICamera* current; zCVob* camVob; bool inCutsceneMode=false,underWater=false; void* dialogCam=nullptr; float rotOffsetX=12,rotOffsetY=15; };
zCAICamera* zCAICamera::current=nullptr;
struct oCInformationManager { bool done=true; static oCInformationManager& GetInformationManager(){static oCInformationManager m;return m;} int HasFinished(){return done;} };
struct zCAIPlayer { enum State{zMV_STATE_STAND,zMV_STATE_FLY,zMV_STATE_SWIM,zMV_STATE_DIVE}; };
struct oCAIHuman { zVEC3 velocity{100,200,300}; oCNpc* npc; int state=zCAIPlayer::zMV_STATE_STAND; float aboveFloor=0; bool canjump=true,standing=true; int jumps=0,runCalls=0,standCalls=0; bool IsInWater(){return false;} int CanJump(int){return canjump;} int JumpForward(){++jumps;canjump=false;standing=false;return 1;} bool IsInCastAni(){return false;} void PC_GoForward(){++runCalls;} int backCalls=0,strafeCalls=0,_t_strafel=7,_t_strafer=8; void PC_GoBackward(){++backCalls;} void StrafeLeft(){++strafeCalls;} void StrafeRight(){++strafeCalls;} bool IsStanding(){return standing;} bool IsWalking(){return !standing;} void _Stand(){++standCalls;standing=true;} };
struct zCTimer { float frameTimeFloat=16.66667f; };
inline zCTimer timer;
inline zCTimer* ztimer=&timer;
inline oCNpc* player=nullptr;
inline oCGame* ogame=nullptr;
inline zCInput_Win32* zinput=nullptr;
constexpr int BS_MOD_TRANSFORMED=4096,BS_MOD_CONTROLLED=2048,BS_MOD_CONTROLLING=8192,BS_ONLY_STATE=127,BS_STAND=0,BS_WALK=1,BS_RUN=3,BS_SPRINT=4,BS_SWIM=5,BS_JUMP=8,BS_CLIMB=9,BS_FALL=10,BS_UNCONSCIOUS=22,BS_CASTING=28;
}
#include "../Plugin/PlayerControls.hpp"
#include <cassert>
#include <iostream>
using namespace Gothic_I_Classic;
int main(){
 oCNpc pc;oCGame game;zCInput_Win32 controls;zCVob camvob;zCAICamera cam;cam.camVob=&camvob;
 player=&pc;ogame=&game;zinput=&controls;zCAICamera::current=&cam;oCAIHuman ai;ai.npc=&pc;
 // HP2 controls: LMB leaves native ACTION (Ctrl stays), RMB leaves native
 // jump (Alt stays) and becomes our jump.
 using V=std::vector<unsigned short>;
 controls.bindings[GAME_SMOVE]={KEY_LMENU,MOUSE_BUTTONRIGHT};
 controls.bindings[GAME_ACTION]={KEY_LCONTROL,MOUSE_BUTTONLEFT};
 controls.bindings[GAME_UP]={KEY_W};
 ParryControls::CaptureInput(&controls);
 assert((controls.bindings[GAME_SMOVE]==V{KEY_LMENU}));
 assert((controls.bindings[GAME_ACTION]==V{KEY_LCONTROL}));
 assert((controls.bindings[GAME_UP]==V{KEY_W}));
 const int bindsAfterEngage=controls.bindCalls;
 ParryControls::CaptureInput(&controls); assert(controls.bindCalls==bindsAfterEngage); // idempotent
 controls.right=true; ParryControls::CaptureInput(&controls);
 assert(ParryControls::GetInput().jumpHeld); // RMB jumps through locomotion
 controls.right=false;
 game.paused=true; ParryControls::CaptureInput(&controls);
 assert((controls.bindings[GAME_SMOVE]==V{KEY_LMENU,MOUSE_BUTTONRIGHT})); // exact order restored for menus
 assert((controls.bindings[GAME_ACTION]==V{KEY_LCONTROL,MOUSE_BUTTONLEFT}));
 game.paused=false; ParryControls::CaptureInput(&controls);
 assert((controls.bindings[GAME_SMOVE]==V{KEY_LMENU}));
 controls.toggled[KEY_F11]=1; ParryControls::CaptureInput(&controls); // F11 native movement
 ParryControls::CaptureInput(&controls);
 assert((controls.bindings[GAME_SMOVE]==V{KEY_LMENU,MOUSE_BUTTONRIGHT})); // native RMB jump back
 assert((controls.bindings[GAME_ACTION]==V{KEY_LCONTROL})); // spells keep LMB
 controls.toggled[KEY_F11]=1; ParryControls::CaptureInput(&controls);
 assert((controls.bindings[GAME_SMOVE]==V{KEY_LMENU}));
 ParryControls::ReleaseNativeBindings();
 assert((controls.bindings[GAME_SMOVE]==V{KEY_LMENU,MOUSE_BUTTONRIGHT}));
 assert((controls.bindings[GAME_ACTION]==V{KEY_LCONTROL,MOUSE_BUTTONLEFT}));
 ParryControls::Reset();
 controls.keys[KEY_W]=1;controls.keys[KEY_D]=1;controls.toggled[KEY_0]=1;
 ParryControls::CaptureInput(&controls);assert(ParryControls::OwnsInput());assert(ParryControls::GetInput().slotPressed==10);assert(controls.states[GAME_UP]==0);
 ParryControls::UpdateCamera(&cam);
 for(int i=0;i<60;++i){ParryControls::BeforeHumanAI(&ai);assert(pc.mdl.aniTransScale[0]==0);ParryControls::AfterHumanAI(&ai);assert(pc.mdl.aniTransScale[0]==1);}
 assert(ai.velocity[0]==0 && ai.velocity[1]==200 && ai.velocity[2]==0);
 assert(pc.position[0]>200&&pc.position[2]>200);assert(pc.position[1]==0);assert(pc.biggestStep<=10.001f);assert(pc.begin==pc.end);
 const int runCallsBeforeJump=ai.runCalls;
 controls.keys[KEY_SPACE]=1;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.jumps==1);
 assert(ai.runCalls==runCallsBeforeJump); // A native jump must not be followed by a run command.
 ParryControls::Disable();assert(cam.rotOffsetX==12&&cam.rotOffsetY==15);
 ai.canjump=true;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.jumps==1);
 controls.keys[KEY_SPACE]=0;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
 controls.keys[KEY_SPACE]=1;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.jumps==2);
 controls.keys[KEY_W]=controls.keys[KEY_D]=controls.keys[KEY_SPACE]=0;
 ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
 const int standCallsBeforeJump=ai.standCalls;
 ai.canjump=true;controls.keys[KEY_SPACE]=1;
 ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
 assert(ai.jumps==3&&ai.standCalls==standCallsBeforeJump&&!ai.standing);
 pc.openinventory=true;ParryControls::CaptureInput(&controls);assert(!ParryControls::OwnsInput());
 pc.openinventory=false;controls.toggled[KEY_F11]=1;ParryControls::CaptureInput(&controls);assert(!ParryControls::OwnsInput());
 assert(ParryControls::ContextAvailable()); // F11 does not disable the spell safety context.
 controls.left=true;controls.states[GAME_ACTION]=1;ParryControls::CaptureInput(&controls);
 assert(!ParryControls::OwnsInput()&&controls.states[GAME_ACTION]==0); // Native action cannot also handle a spell click with F11 off.
 pc.body=BS_UNCONSCIOUS;controls.states[GAME_ACTION]=1;ParryControls::CaptureInput(&controls);
 assert(!ParryControls::ContextAvailable()&&controls.states[GAME_ACTION]==1);
 pc.body=BS_STAND;controls.left=false;controls.states[GAME_ACTION]=1;ParryControls::CaptureInput(&controls);
 assert(controls.states[GAME_ACTION]==1); // Ordinary action input remains native without LMB.
 controls.toggled[KEY_F11]=1;ParryControls::CaptureInput(&controls);assert(ParryControls::OwnsInput());
 controls.left=true;
 for(const int state:{BS_MOD_TRANSFORMED,BS_MOD_CONTROLLED,BS_MOD_CONTROLLING,BS_SWIM,BS_CLIMB,BS_UNCONSCIOUS}){
   pc.body=state;controls.states[GAME_ACTION]=1;ParryControls::CaptureInput(&controls);
   assert(!ParryControls::ContextAvailable()&&!ParryControls::OwnsInput());
   assert(controls.states[GAME_ACTION]==1); // Unsafe states keep native input with F11 on, too.
 }
 controls.right=false;
 pc.body=BS_CASTING;assert(ParryControls::ContextAvailable());
 pc.movLock=true;assert(!ParryControls::ContextAvailable());pc.movLock=false;
 pc.dead=true;assert(!ParryControls::ContextAvailable());pc.dead=false;
 pc.human=false;assert(!ParryControls::ContextAvailable());pc.human=true;
 auto& information=oCInformationManager::GetInformationManager();
 information.done=false;assert(!ParryControls::ContextAvailable());information.done=true;
 cam.dialogCam=&pc;assert(!ParryControls::ContextAvailable());cam.dialogCam=nullptr;
 cam.underWater=true;assert(!ParryControls::ContextAvailable());cam.underWater=false;
 game.paused=true;assert(!ParryControls::ContextAvailable());game.paused=false;
 assert(ParryControls::ContextAvailable());
 ParryControls::Forget();assert(!ParryControls::OwnsInput());
 // Direction-matched animations; the strafe step restarts only once ended.
 {
  controls.left=false;controls.right=false;controls.keys.clear();ai.canjump=false;
  ParryControls::Reset();
  controls.keys[KEY_S]=1;ParryControls::CaptureInput(&controls);
  const int back=ai.backCalls;ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.backCalls==back+1);
  controls.keys.clear();controls.keys[KEY_A]=1;ParryControls::CaptureInput(&controls);
  const int strafes=ai.strafeCalls;ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.strafeCalls==strafes+1);
  pc.mdl.active=true;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.strafeCalls==strafes+1);
  pc.mdl.active=false;
  // Number keys latch until the HUD takes them; repeated polls keep them.
  controls.keys.clear();controls.toggled[KEY_3]=1;ParryControls::CaptureInput(&controls);ParryControls::CaptureInput(&controls);
  assert(ParryControls::GetInput().slotPressed==3);assert(ParryControls::ConsumeSlotPressed()==3);assert(ParryControls::ConsumeSlotPressed()==0);
  ParryControls::Forget();
 }
 std::cout<<"Gothic adapter simulation: movement, jump sequencing, context safety and F11 fallback OK\n";
}
