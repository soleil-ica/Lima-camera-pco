#ifndef PCO_RING_BUFFER_H
#define PCO_RING_BUFFER_H

#include "lima/ThreadUtils.h"
#include <string>

namespace lima
{
namespace Pco
{

struct RingBufferInfo
{
    int requestedFrames;
    int frames;
    int width;
    int height;
    int timestampMode;
    int bitAlignment;
    int dynamicBits;
    bool doubleImage;
    double exposure;
    double latency;
    double framePeriod;
};

class RingBufferControl
{
public:
    /** @brief Allow destruction through the plugin's non-owning controller interface. */
    virtual ~RingBufferControl() {}
    /** @brief Validate and remember the finite history without starting recording. */
    virtual void prepare(int history) = 0;
    /** @brief Switch LImA to unlimited preview immediately before hardware recording. */
    virtual void start() = 0;
    /** @brief Open the private export using the final RAM image count and metadata. */
    virtual void beginExport(const RingBufferInfo& info) = 0;
    /**
     * @brief Consume one individual image synchronously; the SDK retains pixel ownership.
     * @param index Zero-based export position.
     * @param ramIndex One-based PCO RAM index.
     */
    virtual void writeFrame(unsigned short* pixels, int index, unsigned long ramIndex) = 0;
    /** @brief Finalize the export, retaining an incomplete file on cancellation. */
    virtual void endExport(bool cancelled) = 0;
    /** @brief Finish or abort the session, stop LImA and restore its finite history. */
    virtual void completed(bool cancelled, const std::string& error) = 0;
    /** @brief Invalidate camera/control references when the camera is destroyed. */
    virtual void cameraDestroyed() = 0;
};

struct RingBufferState
{
    /** @brief Initialize an idle, disabled session without an export controller. */
    RingBufferState()
        : enabled(false), prepared(false), worker(false), fault(false),
          history(0), completionThread(0), control(0), status("Ring buffer disabled")
    {}

    Cond cond;
    bool enabled;
    bool prepared;
    bool worker;
    bool fault;
    int history;
    pid_t completionThread;
    RingBufferControl* control;
    std::string status;
};

}
}
#endif
