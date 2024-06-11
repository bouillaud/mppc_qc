#include <stdio.h>
#include <stdint.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <fstream>
#include <bitset>
#include <vector>
#include <assert.h>

#ifndef PS6000WRAPPER
#include "libps6000/ps6000Wrapper.h"
#endif

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

using namespace std;
namespace py = pybind11;
// constexpr auto byref = py::return_value_policy::reference_internal;

// typedef enum enBOOL{FALSE,TRUE} BOOL;

class dataCollectionConfig 
{
public:
    ofstream ostream;
    UNIT *unit;
    bitset<4> activeChannels;
    bitset<5> activeTriggers;
    bitset<4> timebase;
    bitset<16> chVoltageRanges;
    vector<int16_t> chTriggerThresholdADC;
    int16_t auxTriggerThresholdADC;
    vector<uint16_t> chPostSamplesPerWaveform; // NOTE: Excludes pre trigger samples
    uint16_t maxPostSamples;
    uint16_t samplesPreTrigger;
    uint32_t numWaveforms;
    vector<vector<int16_t*>> dataBuffers;
    char serial[16];
    dataCollectionConfig(UNIT *unit, char *serial)
    {
        this->unit = unit;
        strcpy(this->serial, serial);
    }
};

dataCollectionConfig g_dcc(NULL, NULL);

void setActiveChannels(dataCollectionConfig &dcc, 
                       int16_t aChVoltage,
                       int16_t bChVoltage,
                       int16_t cChVoltage,
                       int16_t dChVoltage)
{
    int16_t chVoltage[4] = {aChVoltage, bChVoltage, cChVoltage, dChVoltage};
    uint8_t chVRangeBits[4];
    uint16_t allVRange;

    for (int i = 0; i < 4; i++)
    {
        if (chVoltage[i] != 99) 
        {
            dcc.activeChannels.set(i);
        }
        chVRangeBits[i] = chVoltage[i] * dcc.activeChannels.test(i);
    }
    dcc.chVoltageRanges = chVoltage[0] * dcc.activeChannels.test(0) << 12 |
                          chVoltage[1] * dcc.activeChannels.test(1) <<  8 |
                          chVoltage[2] * dcc.activeChannels.test(2) <<  4 |
                          chVoltage[3] * dcc.activeChannels.test(3);

    SetVoltages(dcc.unit, chVoltage);
    return;
}

void setTriggerConfig(dataCollectionConfig &dcc, 
                      int16_t aTrigVoltageMv,
                      int16_t bTrigVoltageMv,
                      int16_t cTrigVoltageMv,
                      int16_t dTrigVoltageMv,
                      int16_t auxTrigVoltageMv)
{

    int16_t trigVoltageMv[5] = {aTrigVoltageMv,
                                bTrigVoltageMv,
                                cTrigVoltageMv,
                                dTrigVoltageMv,
                                auxTrigVoltageMv
                                };

    for (int i = 0; i < 4; i++)
    {
        if (trigVoltageMv[i] != 0 && dcc.activeChannels.test(i))
        {
            dcc.activeTriggers.set(i + 1);
            dcc.chTriggerThresholdADC.push_back(mv_to_adc(trigVoltageMv[i], 
            dcc.unit->channelSettings[PS6000_CHANNEL_A + i].range));
        }
        else
        {
            dcc.chTriggerThresholdADC.push_back(0);
        }
    }
    if (trigVoltageMv[4] != 0)
    {
        dcc.activeTriggers.set(0);
        dcc.auxTriggerThresholdADC = mv_to_adc(trigVoltageMv[4], 
        PS6000_1V);
    }
    else
    {
        dcc.auxTriggerThresholdADC = 0;
    }

    SetTriggers(dcc.unit, dcc.activeTriggers, dcc.chTriggerThresholdADC, dcc.auxTriggerThresholdADC);

    return;
}

void setDataConfig(dataCollectionConfig &dcc, uint8_t *timebase,
    uint32_t *numWaveforms, uint16_t *samplesPreTrigger, uint16_t *chAWfSamples,
    uint16_t *chBWfSamples , uint16_t *chCWfSamples , uint16_t *chDWfSamples)
{
    dcc.timebase = *timebase;
    dcc.numWaveforms = *numWaveforms;
    dcc.samplesPreTrigger = *samplesPreTrigger;

    vector<uint16_t> samplesPerWaveform = {*chAWfSamples, *chBWfSamples, *chCWfSamples, *chDWfSamples};
    dcc.chPostSamplesPerWaveform = samplesPerWaveform;
    dcc.maxPostSamples = *max_element(  dcc.chPostSamplesPerWaveform.begin(),
                                        dcc.chPostSamplesPerWaveform.end());

    dcc.dataBuffers = SetDataBuffers(dcc.unit, dcc.activeChannels, samplesPerWaveform, 
            *samplesPreTrigger, *numWaveforms, dcc.maxPostSamples);

    return;
}

void collectRapidBlockData(dataCollectionConfig &dcc)
{
    uint16_t maxPostTrigger = *max_element( dcc.chPostSamplesPerWaveform.begin(),
                                            dcc.chPostSamplesPerWaveform.end());
    StartRapidBlock(dcc.unit, dcc.samplesPreTrigger, maxPostTrigger, 
            dcc.timebase.to_ulong(), dcc.numWaveforms);
    return;
}

void setDataOutput(dataCollectionConfig &dcc, char *outputFileName)
{
    dcc.ostream.open(outputFileName, ios::out | ios::binary);
    return;
}

bool isLittleEndian()
{
    uint32_t i = 1;
    char *c = (char*)&i;
    return bool(*c);
}

int16_t bswap16(int16_t n)
{
    if (isLittleEndian()) {return __builtin_bswap16(n);}
    else {return n;}
}

uint16_t bswapu16(uint16_t n)
{
    if (isLittleEndian()) {return __builtin_bswap16(n);}
    else {return n;}
}

int32_t bswap32(int32_t n)
{
    if (isLittleEndian()) {return __builtin_bswap32(n);}
    else {return n;}
}

uint32_t bswapu32(uint32_t n)
{
    if (isLittleEndian()) {return __builtin_bswap32(n);}
    else {return n;}
}

template<std::size_t N>
void bitset_reverse(std::bitset<N> &b) 
{
    for(std::size_t i = 0; i < N/2; ++i) 
    {
        bool t = b[i];
        b[i] = b[N-i-1];
        b[N-i-1] = t;
    }
}


void writeDataHeader(dataCollectionConfig &dcc)
{
    // Bit layout, in order
    // 4 bits: timebase (from 0-4 for ps6000)
    // 4 bits: ch1-4 active
    // 3 bits: padding
    // 5 bits: ch1-4, aux trigger active
    // 16 bits: aux trigger threshold
    // 64 (16*4) bits: trigger threshold (ch1-4)
    // 16 (4*4): ch1-4 voltage ranges (aux is always +-1V range)
    // 64 (4*16) bits: number of TOTAL samples per waveform (including pretrigger)
    // 16 bits: number of samples before trigger
    // 32 bits: number of waveforms
    // total above bits: 224 (28 bytes)
    // Flexible length, 0 terminated: model string
    // Flexible length, 0 terminated: serial number

    int16_t o16;
    uint16_t ou16;
    int32_t o32;
    uint32_t ou32;

    bitset_reverse(dcc.activeChannels);
    bitset_reverse(dcc.activeTriggers);

    uint8_t timebaseActiveCh =  (uint8_t) dcc.timebase.to_ullong() << 4 |
                                (uint8_t) dcc.activeChannels.to_ullong();
    dcc.ostream.write((const char *) &timebaseActiveCh, sizeof(uint8_t));

    uint8_t activeTriggers = (uint8_t) dcc.activeTriggers.to_ullong();
    dcc.ostream.write((const char *) &activeTriggers, sizeof(uint8_t));

    o16 = bswap16(dcc.auxTriggerThresholdADC);
    dcc.ostream.write((const char *) &o16, sizeof(int16_t));

    for (int i = 0; i < 4; i++)
    {
        o16 = bswap16(dcc.chTriggerThresholdADC.at(i));
        dcc.ostream.write((const char *) &o16, sizeof(int16_t));
    }

    o16 = bswap16((int16_t) dcc.chVoltageRanges.to_ullong());
    dcc.ostream.write((const char *) &o16, sizeof(int16_t));

    for (int i = 0; i < 4; i++)
    {
        ou16 = bswapu16(dcc.chPostSamplesPerWaveform.at(i) + dcc.samplesPreTrigger);
        dcc.ostream.write((const char *) &ou16, sizeof(uint16_t));
    }

    ou16 = bswapu16(dcc.samplesPreTrigger);
    dcc.ostream.write((const char *) &ou16, sizeof(uint16_t));

    o32 = bswap32(dcc.numWaveforms);
    dcc.ostream.write((const char *) &o32, sizeof(int32_t));

    for (int i = 0; i < sizeof(dcc.unit->modelString); i++)
    {
        dcc.ostream.write((const char *) &dcc.unit->modelString[i], 1L);
        if (dcc.unit->modelString[i] == '\0') {break;}
    }

    dcc.ostream.write((const char *) &dcc.serial, sizeof(&dcc.serial));

    char zero = '\0';
    dcc.ostream.write(&zero, 1L);

    return;
}

void writeDataOut(dataCollectionConfig &dcc)
{
    uint16_t maxSamples = *max_element( dcc.chPostSamplesPerWaveform.begin(), 
                                        dcc.chPostSamplesPerWaveform.end());
    
    int16_t o16;
    uint32_t s = sizeof(int16_t);

    for (int i = 0; i < dcc.activeChannels.count(); i++)
    {
        uint64_t nSamples = (dcc.chPostSamplesPerWaveform.at(i) 
                           + dcc.samplesPreTrigger);
        for (int j = 0; j < dcc.numWaveforms; j++)
        {
            for (int k = 0; k < nSamples; k++)
            {
                o16 = bswap16((dcc.dataBuffers.at(i).at(j))[k]);
                dcc.ostream.write((const char*) &o16, s);
            }
            free(dcc.dataBuffers.at(i).at(j));
        }
    }
}

int initSeriesDaq(char *serial)
{
    UNIT *unit;
    if (serial == "") {serial = NULL;}
    findUnit(unit, (int8_t*) serial);
    try
    {
        g_dcc.unit = unit;
        strcpy(g_dcc.serial, serial);
    }
    catch (exception e)
    {
        printf("Final Catch\n");
        printf("Caught: %s\n", e.what());
        CloseDevice(unit);
        throw e;
    }
    return 1;
}

int seriesSetDaqSettings(
            int16_t chATrigger, int16_t chAVRange, uint16_t chAWfSamples,
            int16_t chBTrigger, int16_t chBVRange, uint16_t chBWfSamples,
            int16_t chCTrigger, int16_t chCVRange, uint16_t chCWfSamples,
            int16_t chDTrigger, int16_t chDVRange, uint16_t chDWfSamples,
            int16_t auxTrigger, uint8_t timebase,
            uint32_t numWaveforms, uint16_t samplesPreTrigger)
{
    setActiveChannels(g_dcc, chAVRange, chBVRange, chCVRange, chDVRange);
    setTriggerConfig(g_dcc, chATrigger, chBTrigger, chCTrigger, chDTrigger, auxTrigger);
    setDataConfig(g_dcc, &timebase, &numWaveforms, &samplesPreTrigger, 
                &chAWfSamples, &chBWfSamples, &chCWfSamples, &chDWfSamples);
}

int seriesCollectData(char *outputFile)
{
    collectRapidBlockData(g_dcc);
    setDataOutput(g_dcc, outputFile);
    writeDataHeader(g_dcc);
    writeDataOut(g_dcc);
}

int seriesCloseDaq()
{
    CloseDevice(g_dcc.unit);
}

// to be run from python side
int runFullDAQ(char *outputFile,
            int16_t chATrigger, int16_t chAVRange, uint16_t chAWfSamples,
            int16_t chBTrigger, int16_t chBVRange, uint16_t chBWfSamples,
            int16_t chCTrigger, int16_t chCVRange, uint16_t chCWfSamples,
            int16_t chDTrigger, int16_t chDVRange, uint16_t chDWfSamples,
            int16_t auxTrigger, uint8_t timebase,
            uint32_t numWaveforms, uint16_t samplesPreTrigger, char *serial)
{
    UNIT *unit;
    if (serial == "") {serial = NULL;}
    findUnit(unit, (int8_t*) serial);
    try
    {
        dataCollectionConfig dcc(unit, serial);
        setActiveChannels(dcc, chAVRange, chBVRange, chCVRange, chDVRange);
        setTriggerConfig(dcc, chATrigger, chBTrigger, chCTrigger, chDTrigger, auxTrigger);
        setDataConfig(dcc, &timebase, &numWaveforms, &samplesPreTrigger, 
                    &chAWfSamples, &chBWfSamples, &chCWfSamples, &chDWfSamples);

        collectRapidBlockData(dcc);
        setDataOutput(dcc, outputFile);
        writeDataHeader(dcc);
        writeDataOut(dcc);
        printf("Data written to %s\n", outputFile);
        CloseDevice(unit);
        printf("Device closed\n");
    }
    catch (exception e)
    {
        printf("Final Catch\n");
        printf("Caught: %s\n", e.what());
        CloseDevice(unit);
        throw e;
    }
    return 1;
}

char* getSerials()
{
    int16_t *serialLth;
    *serialLth = 128;
    char* out = new char[*serialLth];
    int16_t *count;

    enumUnits(count, out, serialLth);

    return out;
}

PYBIND11_MODULE(daq6000, m)
{
    m.doc() = "Picoscope DAQ System";

    m.def("runDAQ", &runFullDAQ, py::return_value_policy::copy); //, py::arg("outputFile"), 
    // py::arg("chATrigger"), py::arg("chAVRange"), py::arg("chAWfSamples"),
    // py::arg("chBTrigger"), py::arg("chBVRange"), py::arg("chBWfSamples"),
    // py::arg("chCTrigger"), py::arg("chCVRange"), py::arg("chCWfSamples"),
    // py::arg("chDTrigger"), py::arg("chDVRange"), py::arg("chDWfSamples"), 
    // py::arg("auxTrigger"), py::arg("timebase"), py::arg("numWaveforms"), 
    // py::arg("samplesPreTrigger"), py::arg("serials"));
    m.def("getSerials", &getSerials, py::return_value_policy::copy);
}

