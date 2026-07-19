#include "meta/MetaMusicScene.h"
#include "os/Debug.h"

MetaMusicScene::MetaMusicScene(DataArray *da) : m_symName(""), mMix(0) { Configure(da); }

MetaMusicScene::~MetaMusicScene() {}

void MetaMusicScene::Configure(DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 0x1A);
    m_symName = i_pConfig->Sym(0);
    static Symbol screens("screens");
    DataArray *screens_found = i_pConfig->FindArray(screens, false);
    if (screens_found) {
        for (int i = 1; i < screens_found->Size(); i++) {
            Symbol sym = screens_found->Sym(i);
            m_lScreens.push_back(sym);
        }
    }
    static Symbol mix("mix");
    mMix = i_pConfig->FindArray(mix, false);
}

Symbol MetaMusicScene::GetName() const { return m_symName; }

const std::list<Symbol> &MetaMusicScene::GetScreenList() const { return m_lScreens; }

// sw2 scatter-include (default/MetaMusicScene <- meta/ConnectionStatusPanel.cpp)
#define gRev gRev_ConnectionStatusPanel
#define gAltRev gAltRev_ConnectionStatusPanel
#include "meta/ConnectionStatusPanel.cpp"
#undef gRev
#undef gAltRev
