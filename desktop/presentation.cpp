// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "presentation.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
namespace slx::desktop {
namespace {
json guide=json::object(),feedback=json::object();
std::filesystem::path feedback_path;
int64_t last_read=0,last_write=0;
int64_t utc_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
// One bounded mailbox keeps storage latency off the OpenGL/rendering thread.
class FrameWriter {
 std::mutex mutex_;std::condition_variable wake_;bool stop_=false;
 std::filesystem::path path_;std::string payload_;std::thread worker_;
 void run(){
  for(;;){
   std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stop_||!payload_.empty();});
   if(stop_)return;
   auto path=path_;auto payload=std::move(payload_);payload_.clear();lock.unlock();
   try{
    auto temp=path;temp+=".tmp";
    {std::ofstream file(temp);file<<payload;if(!file)continue;}
#ifdef _WIN32
    MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
#else
    std::filesystem::rename(temp,path);
#endif
   }catch(...){/* A later frame replaces the failed mailbox update. */}
  }
 }
public:
 FrameWriter():worker_(&FrameWriter::run,this){}
 ~FrameWriter(){{std::lock_guard lock(mutex_);stop_=true;}wake_.notify_one();worker_.join();}
 void publish(const std::filesystem::path &path,std::string value){
  {std::lock_guard lock(mutex_);path_=path;payload_=std::move(value);}wake_.notify_one();
 }
};
}
void presentation_overlay(const std::filesystem::path &root,const json &state,int page,bool replaying){
 const int64_t now=utc_ms();
 if(now-last_read>=100){
  last_read=now;
  try{
   const auto path=root/"workspace/presentation-live.json";
   std::error_code error;const auto bytes=std::filesystem::file_size(path,error);
   if(!error&&bytes<=32768){std::ifstream file(path);json value;file>>value;if(value.value("schema","")=="slx.presentation.live/1")guide=std::move(value);}
  }catch(...){/* An atomic mailbox replacement may coincide with this frame. */}
 }
 const bool active=guide.value("active",false)&&now-guide.value("updated_ms",int64_t(0))>=0&&now-guide.value("updated_ms",int64_t(0))<2500;
 // Recording surfaces remain untouched; instructions live only in Regia.
 if(!active)return;
 const auto token=guide.value("token",std::string());
 feedback_path=root/"workspace/presentation-screen.json";
 feedback={{"schema","slx.presentation.screen/1"},{"session",guide.value("session",std::string())},{"token",token},{"page",page},{"replaying",replaying},{"updated_ms",now},{"values",json::object()}};
 for(const char *key:{"pv_w","house_w","battery_w","ev_w","grid_w","sim_s","hour"})if(state.contains(key))feedback["values"][key]=state[key];
 if(state.contains("config"))for(const char *key:{"cloud","drop_link"})if(state["config"].contains(key))feedback["values"][key]=state["config"][key];
 if(state.contains("devices"))for(const auto &d:state["devices"]){
  const auto id=d.value("id",std::string());if(!id.empty()){feedback["values"][id+"_w"]=d.value("power_w",0.);feedback["values"][id+"_available"]=d.value("available",false);}
 }
}
void presentation_frame_presented(){
 const auto now=utc_ms();
 if(feedback.is_null()||feedback.empty()||now-feedback.value("updated_ms",int64_t(0))>500||now-last_write<100)return;
 static FrameWriter writer;
 writer.publish(feedback_path,feedback.dump());last_write=now;
}
}
