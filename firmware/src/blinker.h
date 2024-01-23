enum blinker_event {
	BLINK_CONNECTED,
	BLINK_DISCONNECTED,
	BLINK_UNPAIRED,
};

void blink(enum blinker_event event);
