using System.Runtime.CompilerServices;

namespace Luca;

internal abstract class LazyTaskBase : INotifyCompletion
{
    public bool IsCompleted { get; set; } = false;
    public Action? Continuation;
    public void OnCompleted(Action continuation) => Continuation = continuation;
}

[AsyncMethodBuilder(typeof(LazyTaskMethodBuilder<>))]
internal class LazyTask<T> : LazyTaskBase
{
    public T? Result;
    public LazyTask<T> GetAwaiter() => this;
    public T GetResult() => Result!;
}

internal struct LazyTaskMethodBuilder<T>
{
    private LazyTask<T> _task;
    public LazyTask<T> Task => _task;
    public static LazyTaskMethodBuilder<T> Create() => new() { _task = new LazyTask<T>() };
    public void SetResult(T result)
    {
        _task.Result = result;
        _task.IsCompleted = true;
        if (_task.Continuation != null)
        {
            Trampoline.Push(_task.Continuation);
        }
    }
    public void SetException(Exception exception) => throw exception;
    public void Start<TStateMachine>(ref TStateMachine stateMachine) where TStateMachine : IAsyncStateMachine
    {
        var sm = stateMachine;
        Trampoline.Push(() => sm.MoveNext());
    }
    public void AwaitOnCompleted<TAwaiter, TStateMachine>(ref TAwaiter awaiter, ref TStateMachine stateMachine)
        where TAwaiter : LazyTask<T>
        where TStateMachine : IAsyncStateMachine
    {
        awaiter.OnCompleted(stateMachine.MoveNext);
    }
    public void AwaitUnsafeOnCompleted<TAwaiter, TStateMachine>(
            ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : ICriticalNotifyCompletion
            where TStateMachine : IAsyncStateMachine
    {
        awaiter.OnCompleted(stateMachine.MoveNext);
    }
    public void SetStateMachine(IAsyncStateMachine stateMachine) { }
}

internal static class Trampoline
{
    private static Stack<Action> _tasks = new();

    public static void Push(Action task)
    {
        _tasks.Push(task);
    }

    public static T Run<T>(LazyTask<T> rootTask)
    {
        while (_tasks.Count > 0)
        {
            var task = _tasks.Pop();
            task();
        }
        if (rootTask.IsCompleted == false)
        {
            throw new InvalidOperationException("The root task is not completed.");
        }
        return rootTask.Result!;
    }
}
