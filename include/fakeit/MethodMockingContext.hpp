/*
 * Copyright (c) 2014 Eran Pe'er.
 *
 * This program is made available under the terms of the MIT License.
 *
 * Created on Mar 10, 2014
 */

#ifndef MethodMockingContext_h__
#define MethodMockingContext_h__

#include <functional>
#include <utility>
#include <type_traits>
#include <tuple>
#include <memory>
#include <vector>
#include <unordered_set>
#include <set>
#include <iosfwd>

#include "fakeit/RecordedMethodBody.hpp"
#include "fakeit/StubbingProgress.hpp"
#include "fakeit/Sequence.hpp"
#include "fakeit/ActualInvocation.hpp"
#include "fakeit/EventHandler.hpp"
#include "fakeit/ActionSequence.hpp"
#include "fakeit/DomainObjects.hpp"
#include "fakeit/SpyingContext.hpp"
#include "fakeit/StubbingContext.hpp"
#include "fakeit/MatchersCollector.hpp"
#include "mockutils/type_utils.hpp"

namespace fakeit {
/**
 * Build recorded sequence and the matching criteria.
 * For example, for the following line:
 * When(Method(mock,foo)).Return(1).Return(2_Times(2)).Throw(e1);
 * The matching criteria is: Any invocation of mock.foo
 * The recorded sequence is: {Return(1), Return(2), Return(2), Throw(e1)}
 */
template<typename R, typename ... arglist>
class MethodMockingContext: //
public Sequence,                // For use in Verify(sequence1,...)... phrases.
		public ActualInvocationsSource, // For use in Using(source1,souece2,...) and VerifyNoOtherInvocations(source1,souece2...) phrases.
		public virtual StubbingContext<R, arglist...>, // For use in Fake, Spy & When phrases
		public virtual SpyingContext<R, arglist...>, // For use in Fake, Spy & When phrases
		private Invocation::Matcher {

public:

	struct Context: public Destructable {

		virtual ~Context() = default;

		/**
		 * Return the original method. not the mock.
		 */
		virtual std::function<R(arglist&...)> getOriginalMethod() = 0;

		virtual std::string getMethodName() = 0;

		virtual void addMethodInvocationHandler(typename ActualInvocation<arglist...>::Matcher* matcher,
				MethodInvocationHandler<R, arglist...>* invocationHandler) = 0;

		virtual void scanActualInvocations(const std::function<void(ActualInvocation<arglist...>&)>& scanner) = 0;

		virtual void setMethodDetails(std::string mockName, std::string methodName) = 0;

		virtual bool isOfMethod(MethodInfo & method) = 0;

		virtual ActualInvocationsSource& getInvolvedMock() = 0;
	};

private:
	class Implementation {

		Context* _stubbingContext;
		ActionSequence<R, arglist...>* _recordedActionSequence;
		typename ActualInvocation<arglist...>::Matcher* _invocationMatcher;bool _commited;

		Context& getStubbingContext() const {
			return *_stubbingContext;
		}

	public:

		Implementation(Context* stubbingContext)
				: _stubbingContext(stubbingContext), _recordedActionSequence(new ActionSequence<R, arglist...>()), _invocationMatcher {
						new DefaultInvocationMatcher<arglist...>() }, _commited(false) {
		}

		~Implementation() {
			delete _stubbingContext;
			if (!_commited) {
				// no commit. delete the created objects.
				delete _recordedActionSequence;
				delete _invocationMatcher;
			}
		}

		ActionSequence<R, arglist...>& getRecordedActionSequence() {
			return *_recordedActionSequence;
		}

		std::string format() const {
			std::string s = getStubbingContext().getMethodName();
			s += _invocationMatcher->format();
			return s;
		}

		void getActualInvocations(std::unordered_set<Invocation*>& into) const {
			auto scanner = [&](ActualInvocation<arglist...>& a) {
				if (_invocationMatcher->matches(a)) {
					into.insert(&a);
				}
			};
			getStubbingContext().scanActualInvocations(scanner);
		}

		/**
		 * Used only by Verify phrase.
		 */
		bool matches(Invocation& invocation) {
			MethodInfo & actualMethod = invocation.getMethod();
			if (!getStubbingContext().isOfMethod(actualMethod)) {
				return false;
			}

			ActualInvocation<arglist...>& actualInvocation = dynamic_cast<ActualInvocation<arglist...>&>(invocation);
			return _invocationMatcher->matches(actualInvocation);
		}

		void commit() {
			getStubbingContext().addMethodInvocationHandler(_invocationMatcher, _recordedActionSequence);
			_commited = true;
		}

		void appendAction(Action<R, arglist...>* action) {
			getRecordedActionSequence().AppendDo(action);
		}

		void setMethodBodyByAssignment(std::function<R(arglist&...)> method) {
			appendAction(new RepeatForever<R, arglist...>(method));
			commit();
		}

		void setMethodDetails(std::string mockName, std::string methodName) {
			getStubbingContext().setMethodDetails(mockName, methodName);
		}

		void getInvolvedMocks(std::set<const ActualInvocationsSource*>& into) const {
			into.insert(&getStubbingContext().getInvolvedMock());
		}

		typename std::function<R(arglist&...)> getOriginalMethod() {
			return getStubbingContext().getOriginalMethod();
		}

		void setInvocationMatcher(typename ActualInvocation<arglist...>::Matcher* matcher) {
			delete _invocationMatcher;
			_invocationMatcher = matcher;
		}
	};

protected:

	MethodMockingContext(Context* stubbingContext)
			: _impl { new Implementation(stubbingContext) } {
	}

	MethodMockingContext(MethodMockingContext&) = default;

	//we have to write move ctor by hand since VC 2013 doesn't support defaulted
	//move constructor and move assignment
	MethodMockingContext(MethodMockingContext&& other) 
			: _impl(std::move(other._impl)) {
	}

	virtual ~MethodMockingContext() {}

	std::string format() const {
		return _impl->format();
	}

	unsigned int size() const override {
		return 1;
	}

	/**
	 * Used only by Verify phrase.
	 */
	void getInvolvedMocks(std::set<const ActualInvocationsSource*>& into) const override {
		_impl->getInvolvedMocks(into);
	}

	void getExpectedSequence(std::vector<Invocation::Matcher*>& into) const override {
		const Invocation::Matcher* b = this;
		Invocation::Matcher* c = const_cast<Invocation::Matcher*>(b);
		into.push_back(c);
	}

	/**
	 * Used only by Verify phrase.
	 */
	void getActualInvocations(std::unordered_set<Invocation*>& into) const override {
		_impl->getActualInvocations(into);
	}

	/**
	 * Used only by Verify phrase.
	 */
	virtual bool matches(Invocation& invocation) override {
		return _impl->matches(invocation);
	}

	virtual void commit() override {
		_impl->commit();
	}

	void setMethodDetails(std::string mockName, std::string methodName) {
		_impl->setMethodDetails(mockName, methodName);
	}

//	void setMatchingCriteria(const arglist&... args) {
//		_impl->setMatchingCriteria(args...);
//	}

	void setMatchingCriteria(std::function<bool(arglist&...)> predicate) {
		typename ActualInvocation<arglist...>::Matcher* matcher { new UserDefinedInvocationMatcher<arglist...>(predicate) };
		_impl->setInvocationMatcher(matcher);
	}

	void setMatchingCriteria(const std::vector<Destructable*>& matchers) {
		typename ActualInvocation<arglist...>::Matcher* matcher { new ArgumentsMatcherInvocationMatcher<arglist...>(matchers) };
		_impl->setInvocationMatcher(matcher);
	}

	/**
	 * Used by Fake, Spy & When functors
	 */
	void appendAction(Action<R, arglist...>* action) override {
		_impl->appendAction(action);
	}

	void setMethodBodyByAssignment(std::function<R(arglist&...)> method) {
		_impl->setMethodBodyByAssignment(method);
	}

	template<class ...matcherCreators, class = typename std::enable_if<sizeof...(matcherCreators)==sizeof...(arglist)>::type>
	void setMatchingCriteria(const matcherCreators& ... matcherCreator) {
		std::vector<Destructable*> matchers;

		MatchersCollector<0, arglist...> c(matchers);
		c.CollectMatchers(matcherCreator...);

		MethodMockingContext<R, arglist...>::setMatchingCriteria(matchers);
	}
private:

	typename std::function<R(arglist&...)> getOriginalMethod() override {
		return _impl->getOriginalMethod();
	}

	std::shared_ptr<Implementation> _impl;
};

template<typename R, typename ... arglist>
class MockingContext: //
public virtual MethodMockingContext<R, arglist...> //
{
	MockingContext & operator=(const MockingContext&) = delete;

public:

	MockingContext(typename MethodMockingContext<R, arglist...>::Context* stubbingContext)
			: MethodMockingContext<R, arglist...>(stubbingContext) {
	}

	MockingContext(MockingContext&) = default;

	MockingContext(MockingContext&& other)
			: MethodMockingContext<R, arglist...>(std::move(other)) {
	}

	virtual ~MockingContext() THROWS {
	}

	MockingContext<R, arglist...>& setMethodDetails(std::string mockName, std::string methodName) {
		MethodMockingContext<R, arglist...>::setMethodDetails(mockName, methodName);
		return *this;
	}

	MockingContext<R, arglist...>& Using(const arglist&... args) {
		MethodMockingContext<R, arglist...>::setMatchingCriteria(args...);
		return *this;
	}

	template<class ...matcherCreator>
	MockingContext<R, arglist...>& Using(const matcherCreator& ... matcherCreators) {
		MethodMockingContext<R, arglist...>::setMatchingCriteria(matcherCreators...);
		return *this;
	}

	MockingContext<R, arglist...>& Matching(std::function<bool(arglist&...)> matcher) {
		MethodMockingContext<R, arglist...>::setMatchingCriteria(matcher);
		return *this;
	}

	MockingContext<R, arglist...>& operator()(const arglist&... args) {
		MethodMockingContext<R, arglist...>::setMatchingCriteria(args...);
		return *this;
	}

	MockingContext<R, arglist...>& operator()(std::function<bool(arglist&...)> matcher) {
		MethodMockingContext<R, arglist...>::setMatchingCriteria(matcher);
		return *this;
	}

	void operator=(std::function<R(arglist&...)> method) {
		MethodMockingContext<R, arglist...>::setMethodBodyByAssignment(method);
	}

	template<typename U = R>
	typename std::enable_if<!std::is_reference<U>::value, void>::type operator=(const R& r) {
		auto method = [r](arglist&...) -> R {return r;};
		MethodMockingContext<R, arglist...>::setMethodBodyByAssignment(method);
	}

	template<typename U = R>
	typename std::enable_if<std::is_reference<U>::value, void>::type operator=(const R& r) {
		auto method = [&r](arglist&...) -> R {return r;};
		MethodMockingContext<R, arglist...>::setMethodBodyByAssignment(method);
	}
};

template<typename ... arglist>
class MockingContext<void, arglist...> :
public virtual MethodMockingContext<void, arglist...> {

	MockingContext & operator=(const MockingContext&) = delete;

public:
	MockingContext(typename MethodMockingContext<void, arglist...>::Context* stubbingContext)
			: MethodMockingContext<void, arglist...>(stubbingContext) {
	}

	virtual ~MockingContext() THROWS {
	}

	MockingContext(MockingContext&) = default;

	MockingContext(MockingContext&& other)
			: MethodMockingContext<void, arglist...>(std::move(other)) {
	}

	void operator=(std::function<void(arglist&...)> method) {
		MethodMockingContext<void, arglist...>::setMethodBodyByAssignment(method);
	}

	MockingContext<void, arglist...>& setMethodDetails(std::string mockName, std::string methodName) {
		MethodMockingContext<void, arglist...>::setMethodDetails(mockName, methodName);
		return *this;
	}

	MockingContext<void, arglist...>& Using(const arglist&... args) {
		MethodMockingContext<void, arglist...>::setMatchingCriteria(args...);
		return *this;
	}

	template<class ...matcherCreator>
	MockingContext<void, arglist...>& Using(const matcherCreator& ... matcherCreators) {
		MethodMockingContext<void, arglist...>::setMatchingCriteria(matcherCreators...);
		return *this;
	}

	MockingContext<void, arglist...>& Matching(std::function<bool(arglist&...)> matcher) {
		MethodMockingContext<void, arglist...>::setMatchingCriteria(matcher);
		return *this;
	}

	MockingContext<void, arglist...>& operator()(const arglist&... args) {
		MethodMockingContext<void, arglist...>::setMatchingCriteria(args...);
		return *this;
	}

	MockingContext<void, arglist...>& operator()(std::function<bool(arglist&...)> matcher) {
		MethodMockingContext<void, arglist...>::setMatchingCriteria(matcher);
		return *this;
	}
};

}
#endif
