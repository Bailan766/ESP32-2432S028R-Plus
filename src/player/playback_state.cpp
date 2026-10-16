#include "playback_state.h"

const char* player_state_to_string(PlayerState state) {
  switch (state) {
    case STATE_STOPPED:  return "已停止";
    case STATE_LOADING:  return "加载中";
    case STATE_PLAYING:  return "播放中";
    case STATE_PAUSED:   return "已暂停";
    case STATE_SEEKING:  return "跳转中";
    case STATE_FINISHED: return "已播完";
    case STATE_ERROR:    return "播放错误";
    default:             return "未知状态";
  }
}

const char* play_mode_to_string(PlayMode mode) {
  switch (mode) {
    case MODE_SEQUENTIAL: return "顺序播放";
    case MODE_REPEAT_ONE: return "单曲循环";
    case MODE_REPEAT_ALL: return "全部循环";
    case MODE_SHUFFLE:    return "随机播放";
    default:              return "默认模式";
  }
}
