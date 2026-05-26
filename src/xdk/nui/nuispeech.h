#pragma once
#include "../win_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _NUI_SPEECH_LANGUAGE {
    NUI_SPEECH_LANGUAGE_EN_US = 0x0000,
    NUI_SPEECH_LANGUAGE_FR_CA = 0x0001,
    NUI_SPEECH_LANGUAGE_EN_GB = 0x0002,
    NUI_SPEECH_LANGUAGE_ES_MX = 0x0003,
    NUI_SPEECH_LANGUAGE_JA_JP = 0x0004,
    NUI_SPEECH_LANGUAGE_FR_FR = 0x0005,
    NUI_SPEECH_LANGUAGE_ES_ES = 0x0006,
    NUI_SPEECH_LANGUAGE_DE_DE = 0x0007,
    NUI_SPEECH_LANGUAGE_IT_IT = 0x0008,
    NUI_SPEECH_LANGUAGE_EN_AU = 0x0009,
    NUI_SPEECH_LANGUAGE_PT_BR = 0x000a,
} NUI_SPEECH_LANGUAGE;

enum NUI_SPEECH_MICTYPE {
    NUI_SPEECH_KINECT = 0x0000,
    NUI_SPEECH_WAVE = 0x0001,
};

typedef enum _NUI_SPEECH_LOADOPTIONS {
    NUI_SPEECH_LOADOPTIONS_STATIC = 0x0000,
    NUI_SPEECH_LOADOPTIONS_DYNAMIC = 0x0001,
} NUI_SPEECH_LOADOPTIONS;

typedef struct _NUI_SPEECH_INIT_PROPERTIES { /* Size=0x8 */
    /* 0x0000 */ NUI_SPEECH_LANGUAGE Language;
    /* 0x0004 */ NUI_SPEECH_MICTYPE MicrophoneType;
} NUI_SPEECH_INIT_PROPERTIES;

typedef struct _NUI_SPEECH_GRAMMAR { /* Size=0x8 */
    /* 0x0000 */ ULONG ulId;
    /* 0x0004 */ HANDLE hGrammar;
} NUI_SPEECH_GRAMMAR;

typedef enum _NUI_SPEECH_GRAMMARSTATE {
    NUI_SPEECH_GRAMMARSTATE_DISABLED = 0x0000,
    NUI_SPEECH_GRAMMARSTATE_ENABLED = 0x0001,
    NUI_SPEECH_GRAMMARSTATE_EXCLUSIVE = 0x0003,
} NUI_SPEECH_GRAMMARSTATE;

typedef enum _NUI_SPEECH_RULESTATE {
    NUI_SPEECH_RULESTATE_INACTIVE = 0x0000,
    NUI_SPEECH_RULESTATE_ACTIVE = 0x0001,
} NUI_SPEECH_RULESTATE;

typedef enum _NUI_SPEECH_CONFIDENCE {
    NUI_SPEECH_CONFIDENCE_LOW = 0x0000,
    NUI_SPEECH_CONFIDENCE_NORMAL = 0x0001,
    NUI_SPEECH_CONFIDENCE_HIGH = 0x0002,
} NUI_SPEECH_CONFIDENCE;

typedef struct _NUI_SPEECH_SEMANTICRESULT { /* Size=0x1c */
    /* 0x0000 */ LPCWSTR pcwszValue;
    /* 0x0004 */ ULONG ulFirstElement;
    /* 0x0008 */ ULONG ulCountOfElements;
    /* 0x000c */ _NUI_SPEECH_SEMANTICRESULT *pFirstChild;
    /* 0x0010 */ _NUI_SPEECH_SEMANTICRESULT *pNextSibling;
    /* 0x0014 */ float fSREngineConfidence;
    /* 0x0018 */ _NUI_SPEECH_CONFIDENCE eConfidence;
} NUI_SPEECH_SEMANTICRESULT;

typedef enum _NUI_SPEECH_WORDTYPE {
    NUI_SPEECH_WORDTYPE_LEXICAL = 0x0001,
    NUI_SPEECH_WORDTYPE_LEXICAL_NO_SPECIAL_CHARS = 0x0003,
} NUI_SPEECH_WORDTYPE;

typedef struct _NUI_SPEECH_SEMANTIC { /* Size=0x4 */
    /* 0x0000 */ LPCWSTR pcwszValue;
} NUI_SPEECH_SEMANTIC;

typedef enum _NUI_SPEECH_INTERFERENCE {
    NUI_SPEECH_INTERFERENCE_NONE = 0x0000,
    NUI_SPEECH_INTERFERENCE_NOISE = 0x0001,
    NUI_SPEECH_INTERFERENCE_NOSIGNAL = 0x0002,
    NUI_SPEECH_INTERFERENCE_TOOLOUD = 0x0003,
    NUI_SPEECH_INTERFERENCE_TOOQUIET = 0x0004,
    NUI_SPEECH_INTERFERENCE_TOOFAST = 0x0005,
    NUI_SPEECH_INTERFERENCE_TOOSLOW = 0x0006,
} NUI_SPEECH_INTERFERENCE;

typedef struct _NUI_SPEECH_RETAINEDAUDIO { /* Size=0x8 */
    /* 0x0000 */ ULONG ulAudioSize;
    /* 0x0004 */ BYTE *pAudioData;
} NUI_SPEECH_RETAINEDAUDIO;

typedef struct _NUI_SPEECH_RULERESULT { /* Size=0x20 */
    /* 0x0000 */ LPCWSTR pcwszName;
    /* 0x0004 */ ULONG ulId;
    /* 0x0008 */ ULONG ulFirstElement;
    /* 0x000c */ ULONG ulCountOfElements;
    /* 0x0010 */ _NUI_SPEECH_RULERESULT *pNextSibling;
    /* 0x0014 */ _NUI_SPEECH_RULERESULT *pFirstChild;
    /* 0x0018 */ float fSREngineConfidence;
    /* 0x001c */ _NUI_SPEECH_CONFIDENCE eConfidence;
} NUI_SPEECH_RULERESULT;

typedef struct _NUI_SPEECH_ELEMENT { /* Size=0x30 */
    /* 0x0000 */ ULONG ulAudioTimeOffset;
    /* 0x0004 */ ULONG ulAudioSizeTime;
    /* 0x0008 */ ULONG ulAudioStreamOffset;
    /* 0x000c */ ULONG ulAudioSizeBytes;
    /* 0x0010 */ ULONG ulRetainedStreamOffset;
    /* 0x0014 */ ULONG ulRetainedSizeBytes;
    /* 0x0018 */ LPCWSTR pcwszDisplayText;
    /* 0x001c */ LPCWSTR pcwszLexicalForm;
    /* 0x0020 */ LPCWSTR pcwszPronunciation;
    /* 0x0024 */ _NUI_SPEECH_CONFIDENCE eRequiredConfidence;
    /* 0x0028 */ _NUI_SPEECH_CONFIDENCE eActualConfidence;
    /* 0x002c */ float fSREngineConfidence;
} NUI_SPEECH_ELEMENT;

typedef struct _NUI_SPEECH_PHRASE { /* Size=0x58 */
    /* 0x0000 */ ULONG cbSize;
    /* 0x0004 */ _NUI_SPEECH_LANGUAGE eLanguage;
    /* 0x0008 */ ULONG ulGrammarID;
    /* 0x0010 */ ULONGLONG ullStartTime;
    /* 0x0018 */ ULONGLONG ullAudioStreamPosition;
    /* 0x0020 */ ULONG ulAudioSizeBytes;
    /* 0x0024 */ ULONG ulAudioSizeTime;
    /* 0x0028 */ ULONG ulRetainedSizeBytes;
    /* 0x002c */ _NUI_SPEECH_RULERESULT Rule;
    /* 0x004c */ ULONG ulCountOfElements;
    /* 0x0050 */ _NUI_SPEECH_ELEMENT *pElements;
    /* 0x0054 */ _NUI_SPEECH_SEMANTICRESULT *pSemanticProperties;
} NUI_SPEECH_PHRASE;

typedef struct _NUI_SPEECH_RECORESULT { /* Size=0x68 */
    /* 0x0000 */ _NUI_SPEECH_PHRASE Phrase;
    /* 0x0058 */ ULONG cAlternates;
    /* 0x005c */ _NUI_SPEECH_PHRASE *pAlternates;
    /* 0x0060 */ _NUI_SPEECH_RETAINEDAUDIO *pAudio;
} NUI_SPEECH_RECORESULT;

typedef struct _NUI_SPEECH_EVENT { /* Size=0x18 */
    /* 0x0000 */ ULONG eventId;
    /* 0x0004 */ ULONG ulStreamNum;
    /* 0x0008 */ ULONGLONG ullAudioStreamOffset;
    union {
        /* 0x0010 */ _NUI_SPEECH_RECORESULT *pResult;
        /* 0x0010 */ void *pvUserContext;
        /* 0x0010 */ _NUI_SPEECH_INTERFERENCE eInterference;
        /* 0x0010 */ _NUI_SPEECH_RETAINEDAUDIO Audio;
    };
} NUI_SPEECH_EVENT;

HRESULT
NuiSpeechGetEvents(ULONG ulCount, NUI_SPEECH_EVENT *pEventArray, ULONG *pulFetched);
HRESULT NuiSpeechDestroyEvent(NUI_SPEECH_EVENT *pEvent);

HRESULT NuiSpeechCommitGrammar(NUI_SPEECH_GRAMMAR *pGrammar);
HRESULT NuiSpeechCreateGrammar(ULONG ulGrammarId, NUI_SPEECH_GRAMMAR *pGrammar);
HRESULT NuiSpeechLoadGrammar(
    LPCWSTR pcwszGrammarName,
    ULONG ulGrammarId,
    NUI_SPEECH_LOADOPTIONS Options,
    NUI_SPEECH_GRAMMAR *pGrammar
);
HRESULT NuiSpeechUnloadGrammar(NUI_SPEECH_GRAMMAR *pGrammar);
HRESULT
NuiSpeechEnable(NUI_SPEECH_INIT_PROPERTIES *pInitProperties, DWORD dwHardwareThread);
HRESULT NuiSpeechDisable();
HRESULT NuiSpeechSetEventInterest(ULONG ulEvents);

HRESULT NuiSpeechEmulateRecognition(LPCWSTR pcwszText, ULONG Options);
HRESULT NuiSpeechStartRecognition();
HRESULT NuiSpeechStopRecognition();
HRESULT
NuiSpeechSetGrammarState(NUI_SPEECH_GRAMMAR *pGrammar, NUI_SPEECH_GRAMMARSTATE eState);
HRESULT NuiSpeechSetRuleState(
    NUI_SPEECH_GRAMMAR *pGrammar, LPCWSTR pcwszRuleName, NUI_SPEECH_RULESTATE eState
);
HRESULT NuiSpeechCreateRule(
    NUI_SPEECH_GRAMMAR *pGrammar,
    LPCWSTR pwszRuleName,
    DWORD dwOptions,
    BOOL fCreateIfNotExist,
    HANDLE phInitialState
);
HRESULT
NuiSpeechCreateState(NUI_SPEECH_GRAMMAR *pGrammar, HANDLE hRule, HANDLE *phNewState);
HRESULT NuiSpeechAddWordTransition(
    NUI_SPEECH_GRAMMAR *pGrammar,
    HANDLE hFromState,
    HANDLE hToState,
    LPCWSTR pcwszWords,
    LPCWSTR pcwszSeparators,
    NUI_SPEECH_WORDTYPE eWordType,
    float fWeight,
    NUI_SPEECH_SEMANTIC *pSemantic
);

#ifdef __cplusplus
}
#endif
