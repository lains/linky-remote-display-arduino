#include "TicFrameParser.h"

TicFrameParser::TicFrameParser(g_ctx_t& ctx) :
        ctx(ctx),
        de(ticFrameParserUnWrapDatasetExtractor, this) {
    this->ctx.tic.nbFramesParsed = 0;
}

#define RESET 0
#define WITHDRAWN_POWER 1
#define RMS_VOLTAGE 2
#define ABS_CURRENT 3

void TicFrameParser::onNewWithdrawnPowerMesurement(uint32_t power) {
    this->mayComputePower(WITHDRAWN_POWER, power);
}

void TicFrameParser::mayComputePower(unsigned int source, unsigned int value) {
    static bool powerKnownForCurrentFrame = false;
    static bool mayInject = false;
    static unsigned int knownRmsVoltage = -1;
    static unsigned int knownAbsCurrent = -1;

    if (source == RESET) {
        if (!powerKnownForCurrentFrame) {
           //this->ctx.tic.lastValidWithdrawnPower = INT32_MIN; /* Unknown power */
        }
        powerKnownForCurrentFrame = false;
        mayInject = false;
        knownRmsVoltage = -1;
        knownAbsCurrent = -1;
        return;
    }
    if (powerKnownForCurrentFrame)
        return;

    if (source == WITHDRAWN_POWER) {
        if (value > 0) {
            powerKnownForCurrentFrame = true;
            if (value > INT32_MAX)
                this->ctx.tic.lastValidWithdrawnPower = INT32_MAX;
            else
                this->ctx.tic.lastValidWithdrawnPower = (int32_t)(value);
            return;
        }
        else { /* Withdrawn power is 0, we may actually inject */
            mayInject = true;
        }
    }
    else if (source == RMS_VOLTAGE) {
        knownRmsVoltage = value;
    }
    else if (source == ABS_CURRENT) {
        knownAbsCurrent = value;
    }
    if (!powerKnownForCurrentFrame &&
        mayInject &&
        knownRmsVoltage != -1 &&
        knownAbsCurrent != -1) {
        /* We are able to estimate the injected power */
        int32_t currentPower = -((int32_t)knownRmsVoltage * (int32_t)knownAbsCurrent); /* Leads to a ngative number because we are injecting */
        powerKnownForCurrentFrame = true;
        if (currentPower <= INT32_MIN)
            this->ctx.tic.lastValidWithdrawnPower = INT32_MIN+1;
        else
            this->ctx.tic.lastValidWithdrawnPower = currentPower;
    }
}

void TicFrameParser::onNewInstVoltageMeasurement(uint32_t voltage) {
    this->mayComputePower(RMS_VOLTAGE, voltage);
}

void TicFrameParser::onNewInstCurrentMeasurement(uint32_t current) {
    this->mayComputePower(ABS_CURRENT, current);
}

/* The 3 methods below are invoked as callbacks by TIC::Unframer and TIC::DatasetExtractor durig the TIC decoding process */
void TicFrameParser::onNewFrameBytes(const uint8_t* buf, unsigned int cnt) {
    this->de.pushBytes(buf, cnt);   /* Forward the bytes to the dataset extractor */
}

void TicFrameParser::onFrameComplete() {
    this->de.reset();
    if (ctx.tic.beat) {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else {
      digitalWrite(LED_BUILTIN, LOW);
    }
    ctx.tic.beat = !ctx.tic.beat;
    this->ctx.tic.nbFramesParsed++;
    this->mayComputePower(RESET, 0);
}

void TicFrameParser::onDatasetExtracted(const uint8_t* buf, unsigned int cnt) {
    /* This is our actual parsing of a newly received dataset */
    TIC::DatasetView dv(buf, cnt);    /* Decode the TIC dataset using a dataset view object */
    if (dv.isValid()) {
        /* Search for SINSTS */
        if (dv.labelSz == 6 &&
            memcmp(dv.labelBuffer, "SINSTS", 6) == 0 &&
            dv.dataSz > 0) {
            /* The current label is a SINSTS with some value associated */
            uint32_t sinsts = dv.uint32FromValueBuffer(dv.dataBuffer, dv.dataSz);
            if (sinsts != (uint32_t)-1)
                this->onNewWithdrawnPowerMesurement(sinsts);
        }
        /* Search for URMS1 */
        else if (dv.labelSz == 5 &&
            memcmp(dv.labelBuffer, "URMS1", 5) == 0 &&
            dv.dataSz > 0) {
            /* The current label is a URMS1 with some value associated */
            uint32_t urms1 = dv.uint32FromValueBuffer(dv.dataBuffer, dv.dataSz);
            if (urms1 != (uint32_t)-1)
                this->onNewInstVoltageMeasurement(urms1);
        }
        /* Search for IRMS1 */
        else if (dv.labelSz == 5 &&
            memcmp(dv.labelBuffer, "IRMS1", 5) == 0 &&
            dv.dataSz > 0) {
            /* The current label is a URMS1 with some value associated */
            uint32_t irms1 = dv.uint32FromValueBuffer(dv.dataBuffer, dv.dataSz);
            if (irms1 != (uint32_t)-1)
                this->onNewInstCurrentMeasurement(irms1);
        }
    }
}
