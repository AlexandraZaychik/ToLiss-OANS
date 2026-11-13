#pragma once
#include <string_view>

namespace oans {
// 0 follows the simulator; only explicit user choices are persisted.
enum class MenuLanguage { automatic, english, chinese };
inline MenuLanguage parse_menu_language(std::string_view value) {
    return value=="english"?MenuLanguage::english:
           value=="chinese"?MenuLanguage::chinese:MenuLanguage::automatic;
}
inline bool menu_uses_chinese(MenuLanguage language,bool simulator_chinese) {
    return language==MenuLanguage::chinese||
           (language==MenuLanguage::automatic&&simulator_chinese);
}
inline const char* localized_menu_label(std::string_view id,const char* english,bool chinese) {
    if(!chinese)return english;
    struct Translation {std::string_view id;const char* text;};
    static constexpr Translation translations[]={
        {"map_window","弹出机场地图"},{"route_plan","弹出滑行路线规划"},
        {"settings","设置"},{"language","语言"},
        {"detail_quality","细节质量"},{"detail_ultra","极致"},{"detail_max","最高"},
        {"detail_xhigh","超高"},{"detail_high","高 (推荐)"},{"detail_medium","中"},{"detail_low","低"},
        {"stand_name","停机位名称"},{"stand_simple","简洁"},{"stand_complex","完整"},
        {"status_bar","显示当前位置"},{"scale_button","使用按钮缩放"},{"cursor","光标样式"},
        {"cursor_pink_vertical","粉色 - 竖向"},{"cursor_pink_horizontal","粉色 - 横向"},
        {"cursor_yellow_vertical","黄色 - 竖向"},{"cursor_yellow_horizontal","黄色 - 横向"},
        {"mcdu2_auto","MCDU2 信息读取"},{"mcdu2_full_open","[吵] MCDU触发"},
        {"mcdu2_with_light","[还行] 灯光触发"},{"mcdu2_full_close","[安静] 基础"},
        {"traffic","显示交通"},{"close_when_takeoff","起飞时关闭机场地图"},
        {"about","关于"},{"test","测试[开发]"}
    };
    for(const auto& translation:translations)if(translation.id==id)return translation.text;
    return english; // Language choices remain English / 中文 in either language.
}
}
