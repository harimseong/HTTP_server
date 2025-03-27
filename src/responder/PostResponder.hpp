#ifndef POSTRESPONDER_HPP
#define POSTRESPONDER_HPP

#include "responder/AResponder.hpp"

class	PostResponder : public AResponder
{
public:
	PostResponder(RequestHandler& requestHandler);
	~PostResponder();

	PostResponder	&operator=(const PostResponder& postResponder);

	virtual void	respondWork();
	void			constructCgi();

private:
};

#endif
