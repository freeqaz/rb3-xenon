#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DuplicatedObject;
    class DataSet;
    class Time;
    class Station;
    class Message;
    class Adapter;
    class Variable;
    class String;
    class CallMethodOperation;
    class RMCContext;

    class DOClass : public RootObject {
    public:
        DOClass(unsigned int);
        virtual ~DOClass();
        virtual unsigned int GetCategory();
        virtual void DataSetsOperation(unsigned int);
        virtual DuplicatedObject *Create() = 0;
        virtual void Delete(DuplicatedObject *) = 0;
        virtual bool ApproveFaultRecovery(DuplicatedObject *) = 0;
        virtual bool ApproveEmigration(DuplicatedObject *, unsigned int) = 0;
        virtual void Trace(DuplicatedObject *, unsigned int) = 0;
        virtual Adapter *CreateAdapter(DuplicatedObject *, unsigned int);
        virtual void DeleteAdapter(DuplicatedObject *, unsigned int, Adapter *);
        virtual void SpecificAddDSToDiscoveryMessage(DuplicatedObject *, Station *, Message *) = 0;
        virtual bool SpecificExtractDSFromDiscoveryMessage(DuplicatedObject *, Message *) = 0;
        virtual bool SpecificExtractADataset(DuplicatedObject *, Message *, unsigned char) = 0;
        virtual bool SpecificUpdate(DuplicatedObject *, DataSet *, const Time &) = 0;
        virtual bool SpecificRefresh(DuplicatedObject *, DataSet *, const Time &) = 0;
        virtual bool IsAKindOf(unsigned int);
        virtual const char *GetClassNameString() const = 0;
        virtual const char *GetDatasetNameString(unsigned char) const;
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual void DispatchRMCResult(RMCContext *);
        virtual bool ValidCastTowards(unsigned int);

        static DOClass *FindDOClass(unsigned int);
    };
}
