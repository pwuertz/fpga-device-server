#ifndef CONTROLHANDLER_H_
#define CONTROLHANDLER_H_

class ControlHandler {
public:
	ControlHandler(const ControlHandler&) = delete;
	ControlHandler& operator=(const ControlHandler&) = delete;
	explicit ControlHandler();

	virtual ~ControlHandler();
};

#endif /* CONTROLHANDLER_H_ */
