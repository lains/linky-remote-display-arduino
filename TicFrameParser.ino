#include "TicFrameParser.h"

TicFrameParser::TicFrameParser(g_ctx_t& ctx) :
        ctx(ctx),
        nbFramesParsed(0),
        de(ticFrameParserUnWrapDatasetExtractor, this) { }

/**
 * @brief Take into account a refreshed instantenous power measurement
 * 
 * @param power The instantaneous power (in Watts)
 */
void TicFrameParser::onNewInstPowerMesurement(uint32_t power) {
    this->ctx.tic.lastValidSinsts = power;
}

/* The 3 methods below are invoked as callbacks by TIC::Unframer and TIC::DatasetExtractor durig the TIC decoding process */
/**
 * @brief Method invoked on new bytes received inside a TIC frame
 * 
 * When receiving new bytes, we will directly forward them to the encapsulated dataset extractor
 * 
 * @param buf A buffer containing new TIC frame bytes
 * @param len The number of bytes stored inside @p buf
 */
void TicFrameParser::onNewFrameBytes(const uint8_t* buf, unsigned int cnt) {
    this->de.pushBytes(buf, cnt);   /* Forward the bytes to the dataset extractor */
}

/**
 * @brief Method invoked when we reach the end of a TIC frame
 * 
 * @warning When reaching the end of a frame, it is mandatory to reset the encapsulated dataset extractor state, so that it starts from scratch on the next frame.
 *          Not doing so would mix datasets content accross two successive frames if we have unterminated datasets, which may happen in historical TIC streams
 */
void TicFrameParser::onFrameComplete() {
    this->de.reset();
    //FIXME: Toggle a LED?
    this->nbFramesParsed++;
}

/**
 * @brief Method invoken when a new dataset has been extracted from the TIC stream
 * 
 * @param buf A buffer containing full TIC dataset bytes
 * @param len The number of bytes stored inside @p buf
 */
void TicFrameParser::onDatasetExtracted(const uint8_t* buf, unsigned int cnt) {
    /* This is our actual parsing of a newly received dataset */
    TIC::DatasetView dv(buf, cnt);    /* Decode the TIC dataset using a dataset view object */
    if (dv.isValid()) {
        if (dv.labelSz == 6 &&
            memcmp(dv.labelBuffer, "SINSTS", 6) == 0 &&
            dv.dataSz > 0) {
            /* The current label is a SINSTS with some value associated */
            uint32_t sinsts = dv.uint32FromValueBuffer(dv.dataBuffer, dv.dataSz);
            if (sinsts != (uint32_t)-1)
                this->onNewInstPowerMesurement(sinsts);
        }
    }
}
