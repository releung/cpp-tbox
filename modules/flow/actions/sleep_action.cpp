#include "sleep_action.h"

#include <tbox/base/assert.h>
#include <tbox/event/loop.h>
#include <tbox/event/timer_event.h>

namespace tbox {
namespace flow {

SleepAction::SleepAction(event::Loop &loop)
  : Action(loop, "Sleep")
  , timer_(loop.newTimerEvent())
{
  TBOX_ASSERT(timer_ != nullptr);
  timer_->setCallback([this] { finish(true); });
}

SleepAction::~SleepAction() {
  delete timer_;
}

void SleepAction::setDuration(const Generator &gen) { gen_ = gen; }

void SleepAction::setDuration(const std::chrono::milliseconds &duration) { time_span_ = duration; }

bool SleepAction::onInit() {
  return \
    (time_span_ != std::chrono::milliseconds::zero()) ||
    (gen_ != nullptr);
}

bool SleepAction::onStart() {
  auto time_span = gen_ ? gen_() : time_span_;

  //! 计算出到期时间点并保存到 finish_time_
  auto now = std::chrono::steady_clock::now();
  finish_time_ = now + time_span;

  timer_->initialize(time_span, event::Event::Mode::kOneshot);
  return timer_->enable();
}

bool SleepAction::onStop() {
  return timer_->disable();
}

bool SleepAction::onPause() {
  //! 计算剩余时长，并保存到 remain_time_span_ 中
  auto now = std::chrono::steady_clock::now();
  remain_time_span_ = std::chrono::duration_cast<std::chrono::milliseconds>(finish_time_ - now);

  return timer_->disable();
}

bool SleepAction::onResume() {
  //! 恢复 remain_time_span_
  timer_->initialize(remain_time_span_, event::Event::Mode::kOneshot);
  return timer_->enable();
}

void SleepAction::onReset() {
  timer_->disable();
}

}
}
