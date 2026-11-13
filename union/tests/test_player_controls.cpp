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
#include <string>
namespace Gothic_I_Classic {
struct zVEC3 { float a[3]; zVEC3(float x=0,float y=0,float z=0):a{x,y,z}{} float& operator[](int n){return a[n];} float operator[](int n)const{return a[n];} };
struct zCVob { zVEC3 at{0,0,1},position; int begin=0,end=0; float biggestStep=0; zVEC3 GetAtVectorWorld(){return at;} zVEC3 GetPositionWorld(){return position;} void SetPositionWorld(zVEC3 p){position=p;} void BeginMovement(){++begin;} void EndMovement(){++end;} void MoveWorld(float x,float y,float z){position[0]+=x;position[1]+=y;position[2]+=z;biggestStep=std::max(biggestStep,std::hypot(x,z));} void SetHeadingYWorld(zVEC3 p){at=p;} void RotateWorldY(float d){float angle=std::atan2(at[0],at[2])+d*0.01745329252f;at=zVEC3(std::sin(angle),0,std::cos(angle));} };
using zSTRING = std::string;
struct zMAT4 {
 float m[4][4]{};
 zMAT4(){for(int i=0;i<4;++i)m[i][i]=1.0f;}
 float* operator[](int i){return m[i];}
 const float* operator[](int i)const{return m[i];}
};
struct zCModelNodeInst { zCModelNodeInst* parentNode=nullptr; zMAT4 trafo; };
struct zCModelAni { int id=0; };
struct zCModel {
 zVEC3 aniTransScale{1,1,1}; zCModelAni ani; bool active=false,isFlying=false;
 int activeId=-1;
 zCModelAni* GetAniFromAniID(int id){ani.id=id;return &ani;}
 int IsAniActive(zCModelAni* a){return active&&(activeId<0||activeId==a->id);}
 void StartAni(int id,int){active=true;activeId=id;}
 zCModelNodeInst* SearchNode(const zSTRING&){return nullptr;}
 zMAT4 GetTrafoNodeToModel(zCModelNodeInst*){return {};}
};
enum zTRigidBodyMode { zPHY_RBMODE_FLY, zPHY_RBMODE_SLIDE };
struct zCRigidBody {
 zVEC3 v; bool gravityOn=true; float gravityScale=1.0f;
 void SetVelocity(zVEC3 p){v=p;}
 void SetMode(zTRigidBodyMode){}
};
struct oCNpc: zCVob {
 bool physicsEnabled=false; zCRigidBody bodyPhysics;
 zCRigidBody* GetRigidBody(){return &bodyPhysics;}
 zVEC3 GetVelocity(){return bodyPhysics.v;}
 bool openinventory=false,dead=false,human=true,movLock=false; int body=0; zCModel mdl;
 bool IsDead(){return dead;} bool IsHuman(){return human;} bool IsMovLock(){return movLock;}
 int GetBodyState(){return body;} void SetBodyState(int s){body=s;} zCModel* GetModel(){return &mdl;}
};
struct oCGame { bool inLoadSaveGame=false,inLevelChange=false,paused=false; bool IsOnPause(){return paused;} };
struct zCConsole { static zCConsole* cur_console; bool visible=false; bool IsVisible(){return visible;} };
zCConsole* zCConsole::cur_console=nullptr;
template<class T> struct zCArray { std::vector<T> v; int GetNumInList() const {return int(v.size());} T& operator[](int i){return v[i];} const T& operator[](int i) const {return v[i];} void InsertEnd(const T& x){v.push_back(x);} };
struct zCInput_Win32 { std::map<int,std::vector<unsigned short>> bindings; int bindCalls=0;
 int IsBinded(unsigned short a,unsigned short k){for(auto x:bindings[a]) if(x==k) return 1; return 0;}
 int GetBinding(unsigned short a,zCArray<unsigned short>& out){out.v=bindings[a];return 1;}
 // Gothic GetState first uses the mapping's forced value, then falls back
 // to bound physical keys. SetState(action, 0) does not suppress held W.
 float NativeState(unsigned short action){
   if(states[action]) return 1.0f;
   for(const auto key:bindings[action]) if(KeyPressed(key)) return 1.0f;
   return 0.0f;
 }
 float GetState(unsigned short action);
 void Bind(unsigned short a,const zCArray<unsigned short>& in){bindings[a]=in.v;++bindCalls;}
 std::map<int,int> keys,toggled,states; bool left=false,right=false; int KeyPressed(int n){return keys[n];} int KeyToggled(int n){int r=toggled[n];toggled[n]=0;return r;} int GetMouseButtonPressedRight(){return right;} int GetMouseButtonPressedLeft(){return left;} void GetMousePos(float&x,float&y,float&z){x=0;y=0;z=0;} int SetState(unsigned short n,int v){states[n]=v;return v;} };
struct zCAICamera { static zCAICamera* current; zCVob* camVob; bool inCutsceneMode=false,underWater=false; void* dialogCam=nullptr; float rotOffsetX=12,rotOffsetY=15; };
zCAICamera* zCAICamera::current=nullptr;
struct oCInformationManager { bool done=true; static oCInformationManager& GetInformationManager(){static oCInformationManager m;return m;} int HasFinished(){return done;} };
struct zCAIPlayer { enum State{zMV_STATE_STAND,zMV_STATE_FLY,zMV_STATE_SWIM,zMV_STATE_DIVE}; };
struct oCAIHuman { zVEC3 velocity{100,200,300}; oCNpc* npc; int state=zCAIPlayer::zMV_STATE_STAND; float aboveFloor=0; bool canjump=true,standing=true,castAnimation=false; int jumps=0,runCalls=0,standCalls=0,_t_stand_2_jump=10,_s_jump=11; struct Config{float zMV_STEP_HEIGHT=30.0f;} config; void StartPhysicsWithVel(){++jumps;standing=false;npc->physicsEnabled=true;npc->bodyPhysics.SetVelocity(velocity);} void StartFallDownAni(){npc->body=10;} bool IsInWater(){return false;} int CanJump(int){return canjump;} int JumpForward(){++jumps;canjump=false;standing=false;return 1;} bool IsInCastAni(){return castAnimation;} void PC_GoForward(){++runCalls;} int backCalls=0,strafeCalls=0,_t_strafel=7,_t_strafer=8; void PC_GoBackward(){++backCalls;} void StrafeLeft(){++strafeCalls;} void StrafeRight(){++strafeCalls;} bool IsStanding(){return standing;} bool IsWalking(){return !standing;} void _Stand(){++standCalls;standing=true;} };
struct zCTimer { float frameTimeFloat=16.66667f; };
inline zCTimer timer;
inline zCTimer* ztimer=&timer;
inline oCNpc* player=nullptr;
inline oCGame* ogame=nullptr;
inline zCInput_Win32* zinput=nullptr;
constexpr int SPL_STATUS_DONTINVEST=0,SPL_STATUS_CANINVEST=1,SPL_STATUS_CAST=2;
constexpr int BS_MOD_TRANSFORMED=4096,BS_MOD_CONTROLLED=2048,BS_MOD_CONTROLLING=8192,BS_ONLY_STATE=127,BS_STAND=0,BS_WALK=1,BS_RUN=3,BS_SPRINT=4,BS_SWIM=5,BS_JUMP=8,BS_CLIMB=9,BS_FALL=10,BS_UNCONSCIOUS=22,BS_CASTING=28;
}
#include "../Plugin/PlayerControls.hpp"
#include <cassert>
#include <iostream>
using namespace Gothic_I_Classic;
static std::uint64_t capturedRelease=0;
static unsigned releaseSnapshots=0;
static void CaptureRelease(std::uint64_t sequence){capturedRelease=sequence;++releaseSnapshots;}
float zCInput_Win32::GetState(unsigned short action) {
 float value=0.0f;
 if(ParryControls::OverrideNativeState(action,value)) return value;
 return NativeState(action);
}
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
 assert(ai.velocity[0]==100 && ai.velocity[1]==200 && ai.velocity[2]==300); // No external native velocity is erased.
 assert(pc.position[0]>200&&pc.position[2]>200);assert(pc.position[1]==0);assert(pc.biggestStep<=10.001f);assert(pc.begin==pc.end);
 // The next GameTick must still see the command after root-motion cleanup.
 assert(ParryControls::lastDesiredX>200 && ParryControls::lastDesiredZ>200);
 const int runCallsBeforeJump=ai.runCalls;
 controls.keys[KEY_SPACE]=1;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.jumps==1);
 assert(ai.runCalls==runCallsBeforeJump); // A native jump must not be followed by a run command.
 ParryControls::Disable();assert(cam.rotOffsetX==12&&cam.rotOffsetY==15);
 ai.canjump=true;pc.physicsEnabled=false;pc.bodyPhysics.v={};pc.body=BS_STAND;pc.mdl.active=false;ai.standing=true;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.jumps==1);
 controls.keys[KEY_SPACE]=0;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
 controls.keys[KEY_SPACE]=1;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.jumps==2);
 controls.keys[KEY_W]=controls.keys[KEY_D]=controls.keys[KEY_SPACE]=0;
 ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
 // The contract double does not simulate flight: explicitly land/reset
 // before checking a fresh third jump. Full flight is tested in hp2_motion.
 ParryControls::Disable();pc.physicsEnabled=false;pc.bodyPhysics.v={};
 pc.body=BS_STAND;pc.mdl.active=false;pc.mdl.isFlying=false;ai.standing=true;
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
  pc.mdl.active=true;pc.mdl.activeId=ai._t_strafel;ParryControls::CaptureInput(&controls);ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);assert(ai.strafeCalls==strafes+1);
  pc.mdl.active=false;
  // Number keys latch until the HUD takes them; repeated polls keep them.
  controls.keys.clear();controls.toggled[KEY_3]=1;ParryControls::CaptureInput(&controls);ParryControls::CaptureInput(&controls);
  assert(ParryControls::GetInput().slotPressed==3);assert(ParryControls::ConsumeSlotPressed()==3);assert(ParryControls::ConsumeSlotPressed()==0);
  ParryControls::Forget();
 }
 // Native player AI must not see the physical movement keys a second time.
 // Merely clearing Gothic's forced mapping leaves physical W visible.
 {
  controls.left=controls.right=false;
  controls.keys.clear();controls.states.clear();
  controls.keys[KEY_W]=controls.keys[KEY_LCONTROL]=1;
  constexpr unsigned short movementActions[]={GAME_UP,GAME_DOWN,GAME_LEFT,
    GAME_RIGHT,GAME_STRAFELEFT,GAME_STRAFERIGHT,GAME_SMOVE,GAME_SMOVE2};
  const auto originalBindings=controls.bindings;
  for(const auto action:movementActions) controls.bindings[action]={KEY_W};
  controls.bindings[GAME_ACTION]={KEY_LCONTROL};
  controls.SetState(GAME_UP,0);
  assert(controls.NativeState(GAME_UP)==1.0f); // regression: old clearing fails
  assert(controls.GetState(GAME_UP)==1.0f); // no global input changes
  ParryControls::SetNativeMagicKeys(false,false);
  ParryControls::Reset();
  {
   ParryControls::NativePlayerInputScope playerInput(true);
   for(const auto action:movementActions) assert(controls.GetState(action)==0.0f);
   assert(controls.GetState(GAME_ACTION)==1.0f); // keyboard interaction remains native
   // Thousands of polls between AI steps used to erase synthetic cast keys.
   // Both physical capture and the scoped magic override now remain stable.
   for(int poll=0;poll<20;++poll){
    ParryControls::CaptureInput(&controls);
    assert(ParryControls::GetInput().forward==1.0f);
    assert(controls.GetState(GAME_UP)==0.0f);
   }
   {
    ParryControls::NativeMagicInputScope magicInput(true);
    ParryControls::SetNativeMagicKeys(true,true);
    for(int poll=0;poll<20;++poll){
     ParryControls::CaptureInput(&controls);
     assert(controls.GetState(GAME_ACTION)==1.0f);
     assert(controls.GetState(GAME_UP)==1.0f);
     assert(controls.GetState(GAME_DOWN)==0.0f);
    }
    controls.keys[KEY_W]=0;
    assert(controls.GetState(GAME_UP)==1.0f); // hold is independent of physical W
    controls.keys[KEY_W]=1;
    ParryControls::SetNativeMagicKeys(false,false);
    assert(controls.GetState(GAME_UP)==0.0f);
    assert(controls.GetState(GAME_ACTION)==0.0f); // release wins over physical Ctrl
    {
     // An NPC AI nested in player callbacks must read its normal input,
     // not inherit either of the player's active input scopes.
     ParryControls::NativePlayerInputScope npcInput(false);
     ParryControls::NativeMagicInputScope npcMagic(false);
     assert(controls.GetState(GAME_UP)==1.0f);
     assert(controls.GetState(GAME_ACTION)==1.0f);
    }
    assert(controls.GetState(GAME_UP)==0.0f);
    assert(controls.GetState(GAME_ACTION)==0.0f);
   }
   assert(controls.GetState(GAME_UP)==0.0f); // player masking restored
   assert(controls.GetState(GAME_ACTION)==1.0f); // magic masking left its scope
  }
  assert(controls.GetState(GAME_UP)==1.0f);
  assert(controls.GetState(GAME_ACTION)==1.0f);
  {
   // F11 restores native movement, but hold/release casting still owns
   // ACTION+UP while the player's MagicMode runs.
   ParryControls::NativePlayerInputScope nativeMovement(false);
   assert(controls.GetState(GAME_UP)==1.0f);
   {
    ParryControls::NativeMagicInputScope magicInput(true);
    ParryControls::SetNativeMagicKeys(false,false);
    assert(controls.GetState(GAME_UP)==0.0f);
    assert(controls.GetState(GAME_ACTION)==0.0f);
    ParryControls::SetNativeMagicKeys(true,true);
    assert(controls.GetState(GAME_UP)==1.0f);
    assert(controls.GetState(GAME_ACTION)==1.0f);
   }
   assert(controls.GetState(GAME_ACTION)==1.0f);
  }
  ParryControls::SetNativeMagicKeys(false,false);
  controls.keys[KEY_W]=controls.keys[KEY_LCONTROL]=0;
  assert(controls.GetState(GAME_UP)==0.0f);
  controls.bindings=originalBindings;
  ParryControls::Forget();
 }
 // Gothic may reach CAST inside Invest and read the status again in the
 // same MagicMode call. Hiding it only before that call is too late: a held
 // LMB could queue a shoot without the player's release request.
 {
  struct NativeSpellCycle {
   int rawStatus=SPL_STATUS_CANINVEST,investedMana=0,nativeInvestCalls=0;
   int queuedShoots=0,casts=0;
   bool prepared=true,requested=false,shootPending=false,finished=false;
   int Status() const {
    return ParryControls::DeferNativeSpellCast(prepared,requested,rawStatus)
      ? SPL_STATUS_CANINVEST:rawStatus;
   }
   void Invest() {
    // The native Invest hook must preserve completed investment, rather
    // than invoke the native implementation again on each held frame.
    if(ParryControls::DeferNativeSpellCast(prepared,requested,rawStatus)) return;
    ++nativeInvestCalls;
    rawStatus=SPL_STATUS_CAST;
    investedMana+=3;
   }
   void MagicModeFrame() {
    if(finished) return;
    if(ParryControls::nativeCastKeys && Status()==SPL_STATUS_CANINVEST) Invest();
    // This second read happens after Invest, in the same native frame.
    if(Status()==SPL_STATUS_CAST && !shootPending){
     ++queuedShoots;
     shootPending=true;
    }
   }
   void FinishShootAnimation() {
    if(shootPending && !finished){++casts;finished=true;}
   }
  } spell;
  assert(spell.Status()==SPL_STATUS_CANINVEST);
  {
   ParryControls::NativeMagicInputScope magicInput(true);
   ParryControls::SetNativeMagicKeys(true,true);
   spell.MagicModeFrame(); // investment completes before the next status read
   assert(spell.rawStatus==SPL_STATUS_CAST && spell.Status()==SPL_STATUS_CANINVEST);
   assert(spell.investedMana==3 && spell.nativeInvestCalls==1);
   assert(spell.queuedShoots==0 && spell.casts==0);
   for(int heldFrame=0;heldFrame<60;++heldFrame) spell.MagicModeFrame();
   assert(spell.rawStatus==SPL_STATUS_CAST && spell.investedMana==3);
   assert(spell.nativeInvestCalls==1 && spell.queuedShoots==0 && spell.casts==0);
   // Losing the reticle target cannot manufacture a release request. Even
   // if logical keys are lowered, the full spell must not shoot by itself.
   ParryControls::SetNativeMagicKeys(false,false);
   for(int lostTargetFrame=0;lostTargetFrame<20;++lostTargetFrame) spell.MagicModeFrame();
   assert(spell.rawStatus==SPL_STATUS_CAST && spell.investedMana==3);
   assert(spell.nativeInvestCalls==1 && spell.queuedShoots==0 && spell.casts==0);
   {
    ParryControls::NativeMagicInputScope npcMagic(false);
    assert(spell.Status()==SPL_STATUS_CAST); // no status masking in nested NPC AI
   }
   assert(spell.Status()==SPL_STATUS_CANINVEST); // scope restored
   spell.prepared=false;
   assert(spell.Status()==SPL_STATUS_CAST); // unrelated/nonprepared spell unchanged
   spell.prepared=true;
   assert(spell.Status()==SPL_STATUS_CANINVEST);
   spell.requested=true; // actual LMB release on a valid NPC
   assert(spell.Status()==SPL_STATUS_CAST);
   spell.MagicModeFrame();
   assert(spell.queuedShoots==1 && spell.casts==0);
   for(int shootFrame=0;shootFrame<10;++shootFrame) spell.MagicModeFrame();
   assert(spell.queuedShoots==1);
   spell.FinishShootAnimation();
   spell.FinishShootAnimation();
   spell.MagicModeFrame();
   assert(spell.queuedShoots==1 && spell.casts==1);
   assert(spell.rawStatus==SPL_STATUS_CAST && spell.investedMana==3);
   assert(spell.nativeInvestCalls==1);
  }
  assert(spell.Status()==SPL_STATUS_CAST); // status hook left native MagicMode
  ParryControls::SetNativeMagicKeys(false,false);
 }
 // With F11 movement restored, a cast still needs temporary movement-key
 // ownership for the whole player AI step, not just inside MagicMode.
 {
  controls.keys.clear();controls.states.clear();controls.left=controls.right=false;
  controls.keys[KEY_W]=1;controls.bindings[GAME_UP]={KEY_W};
  ParryControls::Reset();
  ParryControls::enabled=false;
  ParryControls::CaptureInput(&controls);
  assert(!ParryControls::OwnsInput() && controls.GetState(GAME_UP)==1.0f);
  ParryControls::castPose=true;
  {
   // The adapter passes ownership while driving a cast, despite F11.
   ParryControls::NativePlayerInputScope castingPlayerInput(true);
   assert(controls.GetState(GAME_UP)==0.0f);
   {
    ParryControls::NativeMagicInputScope magicInput(true);
    ParryControls::SetNativeMagicKeys(true,true);
    assert(controls.GetState(GAME_UP)==1.0f);
    ParryControls::SetNativeMagicKeys(false,false);
    assert(controls.GetState(GAME_UP)==0.0f);
   }
   assert(controls.GetState(GAME_UP)==0.0f); // no native walk after MagicMode
  }
  ParryControls::castPose=false;
  assert(controls.GetState(GAME_UP)==1.0f); // normal F11 movement resumes
  controls.keys.clear();
  ParryControls::Reset();
 }
 // Only owned native player AI masks the ten spell hotkeys. UI/native
 // movement, physical polling and a nested NPC never inherit that filter.
 {
  constexpr int spellKeys[]={KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,
                            KEY_6,KEY_7,KEY_8,KEY_9,KEY_0};
  for(const int key:spellKeys) assert(!ParryControls::BlocksNativeSpellHotkey(key));
  {
   ParryControls::NativePlayerInputScope drivingPlayerInput(true);
   for(const int key:spellKeys) assert(ParryControls::BlocksNativeSpellHotkey(key));
   for(const int key:{KEY_W,KEY_SPACE,KEY_F11,MOUSE_BUTTONLEFT})
    assert(!ParryControls::BlocksNativeSpellHotkey(key));
   {
    ParryControls::NativePlayerInputScope physicalPoll(false);
    for(const int key:spellKeys) assert(!ParryControls::BlocksNativeSpellHotkey(key));
   }
   {
    ParryControls::NativePlayerInputScope nestedNpc(false);
    assert(!ParryControls::BlocksNativeSpellHotkey(KEY_1));
   }
   assert(ParryControls::BlocksNativeSpellHotkey(KEY_0));
  }
  // F11 alone/native movement has no owned player input scope.
  assert(!ParryControls::BlocksNativeSpellHotkey(KEY_0));
 }
 // G1 MagicMode's cast latch survives both keys up. A subsequent charge
 // must observe ACTION down + UP up first, even if physical LMB is released.
 // Native addresses: set bit 0x100 at 0x46CBDD, gate at 0x46D363,
 // clear it on ACTION-without-UP at 0x46D3A1. Simulate those engine facts.
 {
  struct NativeCastLatch {
   bool castedLastFrame=false;
   int investments=0;
   void Step(bool action,bool up){
    if(!action) return;
    if(!up) { castedLastFrame=false; return; }
    if(!castedLastFrame) ++investments;
   }
  } native;
  ParryControls::NativePlayerInputScope playerInput(true);
  ParryControls::NativeMagicInputScope magicInput(true);
  const auto tick=[&]{native.Step(controls.GetState(GAME_ACTION)!=0,
                                  controls.GetState(GAME_UP)!=0);};
  ParryControls::SetNativeMagicKeys(true,true);tick();
  assert(native.investments==1);
  native.castedLastFrame=true; // Native Cast finished.
  ParryControls::SetNativeMagicKeys(false,false);tick();
  assert(native.castedLastFrame); // Regression: old coupled release never rearms.
  ParryControls::SetNativeMagicKeys(true,true);tick();
  assert(native.investments==1); // Second spell is blocked by that stale latch.
  ParryControls::SetNativeMagicKeys(true,false);
  assert(controls.GetState(GAME_ACTION)==1.0f && controls.GetState(GAME_UP)==0.0f);
  tick();assert(!native.castedLastFrame);
  ParryControls::SetNativeMagicKeys(true,true);tick();
  assert(native.investments==2); // Fresh charge can invest after the release profile.
  // Pending quick tap requests charge immediately on the next spell, but
  // a scoped native rearm overrides that one step without losing intent.
  native.castedLastFrame=true;
  ParryControls::SetNativeMagicKeys(true,true);
  {
   ParryControls::NativeMagicRearmScope rearm(true);
   assert(controls.GetState(GAME_ACTION)==1.0f && controls.GetState(GAME_UP)==0.0f);
   tick();assert(!native.castedLastFrame && native.investments==2);
   {
    ParryControls::NativeMagicRearmScope other(false);
    assert(controls.GetState(GAME_UP)==1.0f);
   }
   assert(controls.GetState(GAME_UP)==0.0f);
  }
  assert(!ParryControls::nativeMagicRearm);
  tick();assert(native.investments==3); // The held intent resumes after rearm.
  ParryControls::SetNativeMagicKeys(false,false);
 }
 // Prepare/shoot animations can overlap a native walking flag. Neither
 // an idle command nor another ground animation may replace them.
 {
  controls.keys.clear();controls.states.clear();controls.right=false;
  pc.body=BS_CASTING;ai.canjump=false;ai.state=zCAIPlayer::zMV_STATE_STAND;
  ai.aboveFloor=0.0f;ai.castAnimation=true;ai.standing=false;
  ParryControls::Reset();
  const int standCalls=ai.standCalls,runCalls=ai.runCalls;
  const int backCalls=ai.backCalls,strafeCalls=ai.strafeCalls;
  const auto frame=[&]{
   ParryControls::CaptureInput(&controls);
   ParryControls::BeforeHumanAI(&ai);
   ParryControls::AfterHumanAI(&ai);
   assert(ai.standCalls==standCalls && ai.runCalls==runCalls);
   assert(ai.backCalls==backCalls && ai.strafeCalls==strafeCalls);
  };
  controls.left=true;controls.keys[KEY_W]=1;frame(); // prepare while moving
  controls.keys.clear();controls.keys[KEY_S]=1;frame();
  controls.keys.clear();controls.keys[KEY_A]=1;frame();
  controls.left=false;ParryControls::castPose=true;frame(); // pending release
  controls.keys.clear();frame(); // pending shoot, no physical keys left
  ParryControls::castPose=false;frame(); // native recovery still active
  assert(!ai.standing); // no _Stand silently replaced the active animation
  ai.castAnimation=false;ai.standing=true;pc.body=BS_STAND;
  ParryControls::Forget();
 }
 // Console ownership and buffered taps use the real input adapter.
 {
  ParryControls::Forget();ParryControls::enabled=true;
  controls.keys.clear();controls.toggled.clear();controls.left=controls.right=false;
  pc.body=BS_STAND;pc.physicsEnabled=false;pc.bodyPhysics.v={};pc.mdl.active=false;ai.castAnimation=false;ai.standing=true;ai.canjump=true;
  zCConsole console;zCConsole::cur_console=&console;
  ParryControls::CaptureInput(&controls);
  console.visible=true;controls.left=controls.right=true;controls.keys[KEY_W]=1;
  controls.toggled[KEY_F11]=1;controls.toggled[KEY_2]=1;
  ParryControls::CaptureInput(&controls);
  assert(!ParryControls::ContextAvailable()&&!ParryControls::OwnsInput());
  assert(ParryControls::enabled); // F11 is not consumed by the mod in console.
  assert(controls.IsBinded(GAME_ACTION,MOUSE_BUTTONLEFT));
  assert(controls.IsBinded(GAME_SMOVE,MOUSE_BUTTONRIGHT));
  assert(!ParryControls::events.ConsumeRightPressed());
  controls.toggled.clear();console.visible=false;
  ParryControls::CaptureInput(&controls);
  assert(!ParryControls::events.LeftHeld()&&!ParryControls::events.RightHeld());
  controls.left=controls.right=false;controls.keys.clear();
  ParryControls::CaptureInput(&controls);
  controls.right=true;ParryControls::CaptureInput(&controls);
  controls.right=false;ParryControls::CaptureInput(&controls); // full tap before AI
  const int jumps=ai.jumps;
  ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
  assert(ai.jumps==jumps+1);
  ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
  assert(ai.jumps==jumps+1);
  zCConsole::cur_console=nullptr;
  ParryControls::Forget();
 }
 // Snapshot references use the release event itself, even when selecting an
 // empty slot inserts a cancellation before that release in the same poll.
 {
  ParryControls::Forget();ParryControls::enabled=true;
  controls.keys.clear();controls.toggled.clear();controls.left=controls.right=false;
  pc.body=BS_STAND;pc.physicsEnabled=false;pc.bodyPhysics.v={};pc.mdl.active=false;
  ai.standing=true;ai.canjump=true;
  ParryControls::events.Reset(0,false);
  ParryControls::captureReleaseTarget=CaptureRelease;
  ParryControls::CaptureInput(&controls);
  controls.left=true;ParryControls::CaptureInput(&controls);
  const auto beforeRelease=ParryControls::events.LastSequence();
  controls.left=false;controls.toggled[KEY_7]=1;
  ParryControls::CaptureInput(&controls);
  assert(capturedRelease==beforeRelease+2 && releaseSnapshots==1);
  ParryHotter::InputEvent event;
  bool found=false;
  while(ParryControls::events.Pop(event))
   if(event.kind==ParryHotter::InputEventKind::LeftReleased){
    assert(event.sequence==capturedRelease);found=true;
   }
  assert(found);
  ParryControls::CaptureInput(&controls);
  assert(releaseSnapshots==1); // steady polls do not retain another reference
  ParryControls::captureReleaseTarget=nullptr;
  ParryControls::Forget();
 }
 // RMB/Space taps in F11 native movement must never replay as custom jumps.
 {
  controls.keys.clear();controls.toggled.clear();controls.left=controls.right=false;
  pc.body=BS_STAND;pc.physicsEnabled=false;pc.bodyPhysics.v={};pc.mdl.active=false;
  ai.standing=true;ai.canjump=true;
  ParryControls::Reset();ParryControls::CaptureInput(&controls);
  controls.toggled[KEY_F11]=1;ParryControls::CaptureInput(&controls);
  assert(!ParryControls::enabled);
  controls.right=true;controls.keys[KEY_SPACE]=1;
  ParryControls::CaptureInput(&controls);
  controls.right=false;controls.keys[KEY_SPACE]=0;
  ParryControls::CaptureInput(&controls);
  controls.right=true;ParryControls::CaptureInput(&controls);
  controls.toggled[KEY_F11]=1;ParryControls::CaptureInput(&controls);
  const int before=ai.jumps;
  ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
  assert(ai.jumps==before); // held through enabling is not a new request
  controls.right=false;ParryControls::CaptureInput(&controls);
  ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
  controls.right=true;ParryControls::CaptureInput(&controls);
  ParryControls::BeforeHumanAI(&ai);ParryControls::AfterHumanAI(&ai);
  assert(ai.jumps==before+1);
  ParryControls::Forget();
 }
 std::cout<<"Gothic adapter simulation: movement, jump sequencing, context safety and F11 fallback OK\n";
}
