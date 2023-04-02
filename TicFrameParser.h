#pragma once

#include "TicContext.h"

class TicFrameParser {
public:
    /**
     * @brief Default constructor
     */
    TicFrameParser(g_ctx_t& ctx);

    /**
     * @brief Take into account a refreshed instantenous power measurement
     * 
     * @param power The instantaneous power (in Watts)
     */
    void onNewInstPowerMesurement(uint32_t power);

    /* The 3 methods below are invoked as callbacks by TIC::Unframer and TIC::DatasetExtractor durig the TIC decoding process */
    /**
     * @brief Method invoked on new bytes received inside a TIC frame
     * 
     * When receiving new bytes, we will directly forward them to the encapsulated dataset extractor
     * 
     * @param buf A buffer containing new TIC frame bytes
     * @param len The number of bytes stored inside @p buf
     */
    void onNewFrameBytes(const uint8_t* buf, unsigned int cnt);
    
    /**
     * @brief Method invoked when we reach the end of a TIC frame
     * 
     * @warning When reaching the end of a frame, it is mandatory to reset the encapsulated dataset extractor state, so that it starts from scratch on the next frame.
     *          Not doing so would mix datasets content accross two successive frames if we have unterminated datasets, which may happen in historical TIC streams
     */
    void onFrameComplete();

    /**
     * @brief Method invoken when a new dataset has been extracted from the TIC stream
     * 
     * @param buf A buffer containing full TIC dataset bytes
     * @param len The number of bytes stored inside @p buf
     */
    void onDatasetExtracted(const uint8_t* buf, unsigned int cnt);

    /* The 3 commodity functions below are used as callbacks to retrieve a TicFrameParser casted as a context */
    /* They are retrieving our instance on TicFrameParser, and invoking the 3 above corresponding methods of TicFrameParser, forwarding their arguments */
    /**
     * @brief Utility function to unwrap a TicFrameParser instance and invoke onNewFrameBytes() on it
     * It is used as a callback provided to TIC::Unframer
     * 
     * @param buf A buffer containing new TIC frame bytes
     * @param len The number of bytes stored inside @p buf
     * @param context A context as provided by TIC::Unframer, used to retrieve the wrapped TicFrameParser instance
     */
    static void unwrapInvokeOnFrameNewBytes(const uint8_t* buf, unsigned int cnt, void* context) {
        if (context == NULL)
            return; /* Failsafe, discard if no context */
        TicFrameParser* ticFrameParserInstance = static_cast<TicFrameParser*>(context);
        ticFrameParserInstance->onNewFrameBytes(buf, cnt);
    }

    /**
     * @brief Utility function to unwrap a TicFrameParser instance and invoke onFrameComplete() on it
     * It is used as a callback provided to TIC::Unframer
     * 
     * @param context A context as provided by TIC::Unframer, used to retrieve the wrapped TicFrameParser instance
     */
    static void unwrapInvokeOnFrameComplete(void *context) {
        if (context == NULL)
            return; /* Failsafe, discard if no context */
        TicFrameParser* ticFrameParserInstance = static_cast<TicFrameParser*>(context);
        /* We have finished parsing a frame, if there is an open dataset, we should discard it and start over at the following frame */
        ticFrameParserInstance->onFrameComplete();
    }

    /**
     * @brief Utility function to unwrap a TicFrameParser instance and invoke onDatasetExtracted() on it
     * It is used as a callback provided to TIC::DatasetExtractor
     * 
     * @param context A context as provided by TIC::DatasetExtractor, used to retrieve the wrapped TicFrameParser instance
     */
    static void ticFrameParserUnWrapDatasetExtractor(const uint8_t* buf, unsigned int cnt, void* context) {
        if (context == NULL)
            return; /* Failsafe, discard if no context */
        TicFrameParser* ticFrameParserInstance = static_cast<TicFrameParser*>(context);
        /* We have finished parsing a frame, if there is an open dataset, we should discard it and start over at the following frame */
        ticFrameParserInstance->onDatasetExtracted(buf, cnt);
    }

/* Attributes */
    g_ctx_t& ctx; /*!< The global TIC context */
    unsigned int nbFramesParsed; /*!< Total number of complete frames parsed */
    TIC::DatasetExtractor de;   /*!< The encapsulated dataset extractor instance (programmed to call us back on newly decoded datasets) */
};
