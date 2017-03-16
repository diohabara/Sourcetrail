#include "component/controller/UndoRedoController.h"

#include "utility/logging/logging.h"
#include "utility/messaging/type/MessageFlushUpdates.h"
#include "utility/messaging/type/MessageSearch.h"
#include "utility/utility.h"

#include "component/view/UndoRedoView.h"
#include "data/access/StorageAccess.h"

UndoRedoController::UndoRedoController(StorageAccess* storageAccess)
	: m_storageAccess(storageAccess)
{
	m_iterator = m_list.end();
}

UndoRedoController::~UndoRedoController()
{
}

UndoRedoView* UndoRedoController::getView()
{
	return Controller::getView<UndoRedoView>();
}

void UndoRedoController::clear()
{
	m_list.clear();
	m_iterator = m_list.begin();

	getView()->setUndoButtonEnabled(false);
	getView()->setRedoButtonEnabled(false);
}

UndoRedoController::Command::Command(std::shared_ptr<MessageBase> message, Order order, bool replayLastOnly)
	: message(message)
	, order(order)
	, replayLastOnly(replayLastOnly)
{
}

void UndoRedoController::handleMessage(MessageActivateAll* message)
{
	if (sameMessageTypeAsLast(message))
	{
		return;
	}

	Command command(std::make_shared<MessageActivateAll>(*message), Command::ORDER_ACTIVATE);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageActivateLocalSymbols* message)
{
	if (sameMessageTypeAsLast(message))
	{
		static_cast<MessageActivateLocalSymbols*>(lastMessage())->symbolIds = message->symbolIds;
		return;
	}

	Command command(std::make_shared<MessageActivateLocalSymbols>(*message), Command::ORDER_VIEW, true);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageActivateTokens* message)
{
	if (sameMessageTypeAsLast(message) &&
		static_cast<MessageActivateTokens*>(lastMessage())->tokenIds == message->tokenIds)
	{
		return;
	}

	Command command(
		std::make_shared<MessageActivateTokens>(*message),
		message->isEdge ? Command::ORDER_ADAPT : Command::ORDER_ACTIVATE
	);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageChangeFileView* message)
{
	Command command(std::make_shared<MessageChangeFileView>(*message), Command::ORDER_VIEW);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageDeactivateEdge* message)
{
	if (m_iterator == m_list.begin())
	{
		return;
	}

	std::list<Command>::iterator it = m_iterator;
	do
	{
		std::advance(it, -1);
	}
	while (it != m_list.begin() && it->order != Command::ORDER_ACTIVATE);

	MessageBase* m = it->message.get();
	bool keepContent = m->keepContent();

	m->setIsReplayed(false);
	m->setKeepContent(true);
	m->dispatch();

	m->setKeepContent(keepContent);
}

void UndoRedoController::handleMessage(MessageFinishedParsing* message)
{
	std::list<Command> newList;

	for (const Command& command : m_list)
	{
		if (command.order == Command::ORDER_ACTIVATE)
		{
			MessageActivateTokens* msg = dynamic_cast<MessageActivateTokens*>(command.message.get());
			if (msg)
			{
				if (msg->isAggregation)
				{
					continue;
				}
				msg->isFromSearch = false;
			}

			command.message->setIsReplayCleared(true);
			newList.insert(newList.end(), command);
		}
	}

	m_list = newList;
	m_iterator = m_list.end();
}

void UndoRedoController::handleMessage(MessageGraphNodeBundleSplit* message)
{
	Command command(std::make_shared<MessageGraphNodeBundleSplit>(*message), Command::ORDER_ADAPT);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageGraphNodeExpand* message)
{
	Command command(std::make_shared<MessageGraphNodeExpand>(*message), Command::ORDER_VIEW);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageGraphNodeMove* message)
{
	Command command(std::make_shared<MessageGraphNodeMove>(*message), Command::ORDER_VIEW);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageRedo* message)
{
	if (m_iterator == m_list.end())
	{
		return;
	}

	std::list<Command>::iterator oldIterator = m_iterator;

	std::advance(m_iterator, 1);
	while (m_iterator != m_list.end() && m_iterator->order == Command::ORDER_VIEW)
	{
		std::advance(m_iterator, 1);
	}

	getView()->setUndoButtonEnabled(true);
	if (m_iterator == m_list.end())
	{
		getView()->setRedoButtonEnabled(false);
	}

	replayCommands(oldIterator);
}

void UndoRedoController::handleMessage(MessageRefresh* message)
{
	if (!message->uiOnly)
	{
		return;
	}

	if (m_iterator == m_list.begin())
	{
		SearchMatch match = SearchMatch::createCommand(SearchMatch::COMMAND_ALL);
		MessageSearch msg(std::vector<SearchMatch>(1, match));
		msg.dispatch();
	}
	else
	{
		replayCommands();
	}
}

void UndoRedoController::handleMessage(MessageScrollCode* message)
{
	if (sameMessageTypeAsLast(message))
	{
		static_cast<MessageScrollCode*>(lastMessage())->value = message->value;
		return;
	}

	Command command(std::make_shared<MessageScrollCode>(*message), Command::ORDER_VIEW, true);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageScrollGraph* message)
{
	if (sameMessageTypeAsLast(message))
	{
		static_cast<MessageScrollGraph*>(lastMessage())->xValue = message->xValue;
		static_cast<MessageScrollGraph*>(lastMessage())->yValue = message->yValue;
		return;
	}

	Command command(std::make_shared<MessageScrollGraph>(*message), Command::ORDER_VIEW, true);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageSearchFullText* message)
{
	if (sameMessageTypeAsLast(message) &&
		static_cast<MessageSearchFullText*>(lastMessage())->searchTerm == message->searchTerm &&
		static_cast<MessageSearchFullText*>(lastMessage())->caseSensitive == message->caseSensitive)
	{
		return;
	}

	Command command(std::make_shared<MessageSearchFullText>(*message), Command::ORDER_ACTIVATE);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageShowErrors* message)
{
	if (sameMessageTypeAsLast(message) &&
		static_cast<MessageShowErrors*>(lastMessage())->errorId == message->errorId)
	{
		return;
	}

	Command command(std::make_shared<MessageShowErrors>(*message), Command::ORDER_ACTIVATE);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageShowReference* message)
{
	if (sameMessageTypeAsLast(message) &&
		static_cast<MessageShowReference*>(lastMessage())->refIndex == message->refIndex)
	{
		return;
	}

	Command command(std::make_shared<MessageShowReference>(*message), Command::ORDER_VIEW);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageShowScope* message)
{
	Command command(std::make_shared<MessageShowScope>(*message), Command::ORDER_VIEW);
	processCommand(command);
}

void UndoRedoController::handleMessage(MessageUndo* message)
{
	if (!m_list.size())
	{
		return;
	}

	// return to last non view command
	std::list<Command>::iterator it = m_iterator;
	while (std::prev(it)->order == Command::ORDER_VIEW)
	{
		std::advance(it, -1);
	}
	std::advance(it, -1);

	// disable undo button if there is no non view command till the first command
	std::list<Command>::iterator it2 = std::prev(it);
	while (it2->order == Command::ORDER_VIEW)
	{
		std::advance(it2, -1);
	}
	if (it2 == m_list.begin())
	{
		getView()->setUndoButtonEnabled(false);
	}

	// abort if first command is reached
	if (it == m_list.begin())
	{
		return;
	}

	getView()->setRedoButtonEnabled(true);

	m_iterator = it;

	replayCommands();
}

void UndoRedoController::replayCommands()
{
	std::list<Command>::iterator startIterator = m_iterator;

	do
	{
		std::advance(startIterator, -1);
	}
	while (startIterator != m_list.begin() && startIterator->order != Command::ORDER_ACTIVATE);

	replayCommands(startIterator);
}

void UndoRedoController::replayCommands(std::list<Command>::iterator it)
{
	std::vector<std::list<Command>::iterator> viewCommands;
	bool keepsContent = true;

	while (it != m_iterator)
	{
		if (it->order != Command::ORDER_VIEW || it->replayLastOnly == false)
		{
			replayCommand(it);

			if (!it->message->keepContent())
			{
				keepsContent = false;
			}

			if (it->order != Command::ORDER_VIEW)
			{
				viewCommands.clear();
			}
		}
		else
		{
			viewCommands.push_back(it);
		}

		std::advance(it, 1);
	}

	std::set<std::string> messageTypes;
	std::vector<std::list<Command>::iterator> lastViewCommands;

	for (size_t i = viewCommands.size(); i > 0; i--)
	{
		it = viewCommands[i - 1];
		if (messageTypes.find(it->message->getType()) == messageTypes.end())
		{
			messageTypes.insert(it->message->getType());
			lastViewCommands.push_back(it);
		}
	}

	for (size_t i = lastViewCommands.size(); i > 0; i--)
	{
		replayCommand(lastViewCommands[i - 1]);
	}

	MessageFlushUpdates(keepsContent).dispatch();
}

void UndoRedoController::replayCommand(std::list<Command>::iterator it)
{
	std::shared_ptr<MessageBase> m = it->message;
	m->setIsReplayed(true);
	m->setIsLast(it == std::prev(m_iterator));

	if (m->getType() == MessageActivateTokens::getStaticType())
	{
		MessageActivateTokens* msg = dynamic_cast<MessageActivateTokens*>(m.get());

		if (!msg->isEdge && !msg->isAggregation)
		{
			msg->tokenIds = m_storageAccess->getNodeIdsForNameHierarchies(msg->tokenNames);
			msg->searchMatches.clear();
		}
	}

	m->dispatch();
	m->setIsReplayCleared(false);
}

void UndoRedoController::processCommand(Command command)
{
	if (command.message->isReplayed())
	{
		return;
	}

	if (command.order != Command::ORDER_ACTIVATE && m_iterator == m_list.begin())
	{
		return;
	}

	if (command.order == Command::ORDER_ACTIVATE && command.message->keepContent())
	{
		command.order = Command::ORDER_ADAPT;
	}

	if (command.order == Command::ORDER_ACTIVATE)
	{
		m_iterator = m_list.erase(m_iterator, m_list.end());
	}
	else if (command.order == Command::ORDER_ADAPT)
	{
		std::list<Command>::iterator end = m_iterator;
		while (end != m_list.end())
		{
			if (end->order == Command::ORDER_ACTIVATE)
			{
				break;
			}
			std::advance(end, 1);
		}

		m_iterator = m_list.erase(m_iterator, end);
	}

	m_list.insert(m_iterator, command);

	if (command.order != Command::ORDER_VIEW)
	{
		if (m_list.begin() != std::prev(m_iterator))
		{
			getView()->setUndoButtonEnabled(true);
		}

		if (m_list.end() == m_iterator)
		{
			getView()->setRedoButtonEnabled(false);
		}
	}
}

bool UndoRedoController::sameMessageTypeAsLast(MessageBase* message) const
{
	if (!m_list.size() || m_list.begin() == m_iterator)
	{
		return false;
	}

	return lastMessage()->getType() == message->getType();
}

MessageBase* UndoRedoController::lastMessage() const
{
	return std::prev(m_iterator)->message.get();
}

void UndoRedoController::dump() const
{
	std::cout << "\nUndo Redo Stack:\n----------\n";

	std::list<Command>::const_iterator it = m_list.begin();
	while (it != m_list.end())
	{
		switch (it->order)
		{
			case Command::ORDER_VIEW:
				std::cout << "\t";
			case Command::ORDER_ADAPT:
				std::cout << "\t";
			case Command::ORDER_ACTIVATE:
				break;
		}

		std::cout << it->message->getType();

		if (it == m_iterator)
		{
			std::cout << " <-";
		}
		std::cout << std::endl;

		std::advance(it, 1);
	}

	if (m_list.end() == m_iterator)
	{
		std::cout << "<-" << std::endl;
	}
	std::cout << "----------" << std::endl;
}
