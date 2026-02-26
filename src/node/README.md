# src/node/

[`src/node/`](./) 디렉터리는 노드 상태에 접근해야 하는 코드(예: `CChain`, 
`CBlockIndex`, `CCoinsView`, `CTxMemPool` 및 유사 클래스의 상태)를 담고 있음.

[`src/node/`](./) 안의 코드는 [`src/wallet/`](../wallet/) 및 [`src/qt/`](../qt/)
안의 코드와 분리되도록 설계되어 있음. 이는 지갑 및 GUI 코드 변경이 노드 동작에 
영향을 주지 않도록 하기 위함이며 지갑/GUI 코드를 별도의 프로세스에서 실행할 수 
있게 하고 장기적으로는 지갑/GUI 코드가 별도의 소스 저장소에서 유지보수 될 가능성
까지 염두에 둔 것임.

경험칙으로, [`src/node/`](./), [`src/wallet/`](../wallet/), [`src/qt/`](../qt/) 
중 한 디렉터리에 있는 코드는 다른 디렉터리의 코드를 **직접 호출하는 것을 피해야** 
하며, 대신 더 제한된 [`src/interfaces/`](../interfaces/) 클래스들을 통해 
**간접적으로만** 호출하는 것이 바람직함.

현재 이 디렉터리는 파일 구성이 비교적 드문 편임.
향후에는 [`src/validation.cpp`](../validation.cpp)나 
[`src/txmempool.cpp`](../txmempool.cpp) 같은 더 핵심적인 파일들이 이곳으로 
옮겨질 수도 있음.


ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ

# src/node/

The [`src/node/`](./) directory contains code that needs to access node state
(state in `CChain`, `CBlockIndex`, `CCoinsView`, `CTxMemPool`, and similar
classes).

Code in [`src/node/`](./) is meant to be segregated from code in
[`src/wallet/`](../wallet/) and [`src/qt/`](../qt/), to ensure wallet and GUI
code changes don't interfere with node operation, to allow wallet and GUI code
to run in separate processes, and to perhaps eventually allow wallet and GUI
code to be maintained in separate source repositories.

As a rule of thumb, code in one of the [`src/node/`](./),
[`src/wallet/`](../wallet/), or [`src/qt/`](../qt/) directories should avoid
calling code in the other directories directly, and only invoke it indirectly
through the more limited [`src/interfaces/`](../interfaces/) classes.

This directory is at the moment
sparsely populated. Eventually more substantial files like
[`src/validation.cpp`](../validation.cpp) and
[`src/txmempool.cpp`](../txmempool.cpp) might be moved there.
