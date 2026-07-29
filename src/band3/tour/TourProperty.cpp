#include "tour/TourProperty.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

TourProperty::TourProperty(const DataArray *da)
    : unk_0x8(0x20), unk_0x9(0x20), mDefaultValue(0),
      mMinValue(1.17549435082228750797e-38), mMaxValue(3.40282346638528859812e38),
      mIsAutomatic(false) {
    Configure(da);
}

TourProperty::~TourProperty() {}

void TourProperty::Configure(const DataArray *i_pConfig) {
    // Retail spells these six as FUNCTION-LOCAL statics (one guard word,
    // 0x82CBEC2C, six bits + six `??__F` funclets at 0x82369208..0x823692C8),
    // declared at point of use.  See TourDesc.cpp for the same pattern.
    MILO_ASSERT(i_pConfig, 32);
    mName = i_pConfig->Sym(0);
    static Symbol default_value("default_value"); // bit 0
    i_pConfig->FindData(default_value, mDefaultValue, false);
    static Symbol min_value("min_value"); // bit 1
    i_pConfig->FindData(min_value, mMinValue, false);
    static Symbol max_value("max_value"); // bit 2
    i_pConfig->FindData(max_value, mMaxValue, false);
    static Symbol positive_icon("positive_icon"); // bit 3
    const char *a = NULL, *b;
    if (i_pConfig->FindData(positive_icon, a, false)) {
        unk_0x8 = *a;
    }
    static Symbol negative_icon("negative_icon"); // bit 4
    b = NULL;
    if (i_pConfig->FindData(negative_icon, b, false)) {
        unk_0x9 = *b;
    }
    static Symbol is_automatic("is_automatic"); // bit 5
    i_pConfig->FindData(is_automatic, mIsAutomatic, false);
}

Symbol TourProperty::GetName() const { return mName; }

float TourProperty::GetDefaultValue() const { return mDefaultValue; }
float TourProperty::GetMinValue() const { return mMinValue; }
float TourProperty::GetMaxValue() const { return mMaxValue; }
bool TourProperty::IsAutomatic() const { return mIsAutomatic; }
