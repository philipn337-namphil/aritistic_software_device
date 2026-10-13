#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\InputSession.h"
#pragma once
#include "ProductView.h"

namespace routine::application {
// Context describes a frame actually presented on the LCD, not merely the
// newest in-memory view. Keep cosmetic redraws on the same context.
class InputSession {
 public:
  bool matches(const ProductState& state,const ProductView& view)const{
    return valid_&&sameInputTarget(view_,view)&&run_==runId(state,view);
  }
  void publish(const ProductState& state,const ProductView& view,bool transient){
    if(!matches(state,view)||transient_!=transient){if(++context_==0)++context_;}
    view_=view;run_=runId(state,view);transient_=transient;valid_=true;
  }
  void invalidate(){valid_=false;}
  uint32_t context()const{return valid_?context_:0;}
  bool accepts(uint32_t context,const ProductState& state,const ProductView& view)const{
    return context&&context==this->context()&&matches(state,view);
  }
  bool transient()const{return transient_;}
 private:
  static domain::RunId runId(const ProductState& state,const ProductView& view){
    return view.routine>=0&&view.routine<state.count?state.routines[size_t(view.routine)].run.spec.id:0;
  }
  ProductView view_{};
  domain::RunId run_=0;
  uint32_t context_=0;
  bool valid_=false,transient_=false;
};
}
