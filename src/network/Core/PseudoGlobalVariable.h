#pragma once
#include "Platform/MemoryManager.h"
#include "Core/PseudoSingleton.h"
#include "Platform/RootObject.h"
#include "Plugins/StreamSettings.h"

namespace Quazal {
    class PseudoGlobalVariableRoot;

    class PseudoGlobalVariableList : public RootObject {
    public:
        PseudoGlobalVariableList();
        virtual ~PseudoGlobalVariableList();

        void AddVariable(PseudoGlobalVariableRoot *);
        void RemoveVariable(PseudoGlobalVariableRoot *);

        static unsigned int m_uiNbOfVariables;
        static PseudoGlobalVariableRoot *s_pVariableListHead;
    };

    class PseudoGlobalVariableRoot : public RootObject {
    public:
        PseudoGlobalVariableRoot();
        virtual ~PseudoGlobalVariableRoot();
        virtual void AllocateExtraContexts() = 0;
        virtual void FreeExtraContexts() = 0;
        virtual void ResetContext(unsigned int) = 0;
        virtual PseudoGlobalVariableRoot *GetNext() = 0;
        virtual void SetNext(PseudoGlobalVariableRoot *) = 0;

        static PseudoGlobalVariableList s_oList;
        static unsigned int s_uiNbOfExtraContexts;

        PseudoGlobalVariableRoot *mNext; // 0x4
    };

    template <class T>
    class PseudoGlobalVariable : public PseudoGlobalVariableRoot {
    public:
        PseudoGlobalVariable(const T &item = T()) {
            mDefaultValue = item;
            mValueInDefaultContext = mDefaultValue;
            mValueInContextList = 0;
            s_oList.AddVariable(this);
            if (s_uiNbOfExtraContexts > 1) {
                AllocateExtraContexts();
            }
        }
        virtual ~PseudoGlobalVariable() {
            s_oList.RemoveVariable(this);
            FreeExtraContexts();
        }
        virtual void AllocateExtraContexts() {
            // Retail (0x82A7DF58, the DOHandle instantiation) evaluates an
            // array new whose result is never stored when the count is -1, then
            // allocates the context list and copy-constructs the default value
            // into every slot.
            if (s_uiNbOfExtraContexts == -1) {
                new T[s_uiNbOfExtraContexts];
            }
            unsigned int uiSize = s_uiNbOfExtraContexts * sizeof(T);
            mValueInContextList = (T *)QUAZAL_DEFAULT_ALLOC(uiSize, 0x77, _InstType10);
            for (unsigned int i = 0; i < s_uiNbOfExtraContexts; i++) {
                new (&mValueInContextList[i]) T(mDefaultValue);
            }
        }
        virtual void FreeExtraContexts() {
            if (mValueInContextList) {
                for (int i = 0; i < s_uiNbOfExtraContexts; i++) {
                    mValueInContextList[i].~T();
                }
                T *pList = mValueInContextList;
                QUAZAL_DEFAULT_FREE(pList, _InstType10);
                mValueInContextList = 0;
            }
        }
        virtual void ResetContext(unsigned int idx) {
            mValueInContextList[idx] = mDefaultValue;
        }
        virtual PseudoGlobalVariableRoot *GetNext() { return mNext; }
        virtual void SetNext(PseudoGlobalVariableRoot *root) { mNext = root; }

        void SetValue(const T &value) {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            if (uiContext == 0) {
                mValueInDefaultContext = value;
            } else {
                mValueInContextList[uiContext] = value;
            }
        }

        T &GetValue() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            if (uiContext == 0) {
                return mValueInDefaultContext;
            } else {
                return mValueInContextList[uiContext];
            }
        }

        T *mValueInContextList;
        T mValueInDefaultContext;
        T mDefaultValue;
    };
}