#pragma once

namespace modular {

class TransportController {
public:
    TransportController();

    void update(float deltaTimeSeconds);

    void play();
    void pause();
    void stop();

    bool isPlaying() const;
    bool isOnline() const;

    void setOnline(bool online);

    void setBpm(float bpm);
    float getBpm() const;

    float getBeatsElapsed() const;
    float getNormalizedPhase() const;

    void resetPhase();

private:
    bool online_;
    bool playing_;
    float bpm_;
    float beatsElapsed_;
    float loopLengthBeats_;
};

} // namespace modular
