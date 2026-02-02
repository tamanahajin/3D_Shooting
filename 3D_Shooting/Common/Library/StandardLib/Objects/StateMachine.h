#pragma once
#include "stdafx.h"

namespace shooting {

	template<typename T>
	class StateMachine;

	/// <summary>
	/// ステート実装テンプレートクラス(抽象クラス)
	/// </summary>
	/// <typeparam name="T"></typeparam>
	template<typename T>
	class ObjState
	{
	public:
		ObjState() {}
		virtual ~ObjState() {}
		/// <summary>
		/// ステートに入ったときに実行される
		/// </summary>
		/// <param name="Obj">ステートを保持するオブジェクト</param>
		virtual void Enter(const std::shared_ptr<T>& obj) = 0;
		/// <summary>
		/// Updateのときに実行される
		/// </summary>
		/// <param name="Obj">ステートを保持するオブジェクト</param>
		virtual void Execute(const std::shared_ptr<T>& obj) = 0;
		/// <summary>
		/// ステートを出るときに実行される
		/// </summary>
		/// <param name="obj">ステートを保持するオブジェクト</param>
		virtual void Exit(const std::shared_ptr<T>& obj) = 0;
	};

	template <typename T>
	class StateMachine
	{
	private:
		//このステートマシンを持つオーナー
		std::weak_ptr<T> m_Owner;
		//現在のステート
		std::weak_ptr< ObjState<T> > m_CurrentState;
		//一つ前のステート
		std::weak_ptr< ObjState<T> > m_PreviousState;
	public:
		explicit StateMachine(const std::shared_ptr<T>& owner) :
			m_Owner(owner)
		{
		}
		virtual ~StateMachine() {}
		void SetCurrentState(const std::shared_ptr< ObjState<T> >& s) { m_CurrentState = s; }
		void SetPreviousState(const std::shared_ptr< ObjState<T> >& s) { m_PreviousState = s; }
		std::shared_ptr< ObjState<T> >  GetCurrentState() const
		{
			auto shptr = m_CurrentState.lock();
			if (shptr)
			{
				return shptr;
			}
			return nullptr;
		}
		std::shared_ptr< ObjState<T> >  GetPreviousState()const
		{
			auto shptr = m_PreviousState.lock();
			if (shptr)
			{
				return shptr;
			}
			return nullptr;
		}
		void Update() const
		{
			auto shptr = m_CurrentState.lock();
			auto ow_shptr = m_Owner.lock();
			if (shptr && ow_shptr)
			{
				shptr->Execute(ow_shptr);
			}
		}
		void Update2() const
		{
			auto shptr = m_CurrentState.lock();
			auto ow_shptr = m_Owner.lock();
			if (shptr && ow_shptr)
			{
				shptr->Execute2(ow_shptr);
			}
		}
		void  ChangeState(const std::shared_ptr< ObjState<T> >& newState)
		{
			//元のステートを保存
			m_PreviousState = m_CurrentState;
			auto shptr = m_CurrentState.lock();
			auto ow_shptr = m_Owner.lock();
			if (shptr && ow_shptr)
			{
				//元のステートに終了を知らせる
				shptr->Exit(ow_shptr);
			}
			//新しいステートをカレントに設定
			m_CurrentState = newState;
			shptr = m_CurrentState.lock();
			if (shptr && ow_shptr)
			{
				//新しいステートに開始を知らせる
				shptr->Enter(ow_shptr);
			}
		}
		void RevertToPreviousState()
		{
			ChangeState(m_PreviousState);
		}
		/// <summary>
		/// カレントステートが指定のステートかどうか調べる
		/// </summary>
		/// <param name="st"></param>
		/// <returns></returns>
		bool IsInState(const std::shared_ptr< ObjState<T> >& st)const
		{
			if (st == GetCurrentState())
			{
				return true;
			}
			return false;
		}
	};
}