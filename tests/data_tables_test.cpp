#include "deimos/data_tables.hpp"
#include <cassert>

int main() {
    using namespace deimos;
    std::string error;
    auto iddoc=parse_tagged_text("#Air Layer <air >\r#Ground Layer <grnd>\r",&error);
    auto ids=parse_id_list(*iddoc,&error); assert(ids && ids->size()==2 && (*ids)[0].second.str()=="air ");
    auto fdoc=parse_tagged_text("#FPS_MaxRate <30.0>\r#Particle_Gravity <0.96>\r",&error);
    auto floats=parse_float_list(*fdoc,&error); assert(floats && (*floats)[0].second==30.0f);
    auto cdoc=parse_tagged_text("#scoreBar_Digit <52c594>\r",&error);
    auto colors=parse_color_list(*cdoc,&error); assert(colors && (*colors)[0].second.red==0x52);
    auto rdoc=parse_tagged_text("#Score Area <25, 81, 135, 95>\r",&error);
    auto rects=parse_rect_list(*rdoc,&error); assert(rects && (*rects)[0].second.right==135);
    auto sdoc=parse_tagged_text("One\rTwo\rThree\r",&error);
    auto strings=parse_string_list(*sdoc,&error); assert(strings && strings->size()==3 && (*strings)[2]=="Three");

    auto tdoc=parse_tagged_text(
        "#Loc_X_INT <416>\r#Loc_Y_INT <87>\r#Size_INT <0>\r#Format_ID <LEFT>\r"
        "#Monospaced_BOOL <FALSE>\r#DrawShadows_BOOL <FALSE>\r#BlendAmount_0To32_INT <0>\r"
        "#SpaceBetweenChars_INT <0>\r#Colorise_Do_BOOL <FALSE>\r#ColoriseColor_RGB <000000>\r"
        "#ColorStrip_Do_BOOL <TRUE>\r#ColorStrip_HOffset_INT <0>\r#ColorStrip_VOffset_INT <0>\r"
        "#ColorStrip_BlendAmount_0To32_INT <14>\r#ColorStrip_Color_RGB <000000>\r"
        "#ColorStrip_MinWidth_INT <0>\r#ColorStrip_MinHeight_INT <0>\r", &error);
    auto tf=parse_text_format(*tdoc,&error); assert(tf && tf->x==416 && tf->format_token=="LEFT" && tf->color_strip);
    return 0;
}
