# QOR-0022: What assistants may do with Unreal Engine, after Epic's change

- **Status:** Proposed. It takes effect only when the owner accepts it. Until then, `AGENTS.md` §3 and
  [QOR-0020](QOR-0020-unreal-engine-on-qor-os.md)'s decision 8 stand unchanged, and assistants keep to them. Accepting Epic's or
  anyone's terms stays the owner's act, and this record does not do it.
- **Date:** 2026-11-01
- **Deciders:** the owner. Drafted by the assistant, as the owner's brief of 1 November 2026 asked.
- **Would supersede:** QOR-0020's decision 8, and the sentence on assistants in `AGENTS.md` §3
- **Relates to:** [QOR-0021](QOR-0021-projects-engines-and-two-stores.md)

## Context

**QOR-0020 missed this.** It was written on 1 November from Epic's licence of May 2023 and captures from before the change. The
owner reports that Epic's official
[change log](https://www.unrealengine.com/eula-change-log/unreal) lists **update 22, dated 30 October 2026, removing the old section
6(e) generative-AI and MetaHuman restrictions.** That settles what the secondary reports disagreed on, and QOR-0020's rule rests on
a clause that has gone.

**Not read by the assistant:** the current [EULA](https://www.unrealengine.com/eula/unreal), that change log, the
[Epic Content License](https://www.unrealengine.com/eula/content) and [Fab's EULA](https://www.fab.com/eula) answer 403 to its
automated reading, and it has not tried to get around that. Everything below that depends on their wording is marked.

**Not established by the published change:** which agreement the owner accepted, or what separate content terms permit. This PC
has no Epic launcher record and no engine folder.

**Four sets of terms stay apart:** the Engine licence, the Epic Content License, each Fab item's licence, and any custom terms.
Content acquired earlier [may keep the terms of its day](https://wiki.licenseware.io/wiki/fab-and-epic-content-licensing/) (a secondary
source). [Fab's documentation](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab?lang=en-US) says a NoAI tag means
an asset "must not be used for generative AI data collection", and a search listing of the
[Content License's change log](https://www.unrealengine.com/eula-change-log/content) shows NoAI content barred "as inputs to
Generative AI Programs" as well.

## Decision

Proposed: one rule for each operation, in place of the blanket one. A restriction on one is not a ban on the others, and most QOR
OS work involves none of them.

| Operation | Terms it turns on | Proposed rule | Still uncertain |
|-----------|-------------------|---------------|-----------------|
| Running an editor the person installed | Engine licence, the person's own | Their choice. No code of ours is involved. | Nothing for us |
| Starting that editor or Epic's build tool from QQ Studio, by a person's action | Engine licence | Allowed. We ship none of Epic's code. | That the current text has no new limit: section 8(a) |
| An agent automating a user-installed editor | Engine licence; each asset's terms | Allowed for cleared assets, once the conditions below hold. Permission is not access: whether an agent has real editor control is reported separately. | Section 8(a); the owner's agreement |
| An agent reading project code, build logs and Epic's public documents | Engine licence; the agent provider's terms | Allowed once the conditions below hold. | Section 6(e) as it now reads; the provider's settings |
| An agent reading engine source | Engine licence; the provider's terms | The same, only where a task needs it. The engine is still never in this repository. | The same |
| Training a model on the engine, projects or assets | Engine licence; content terms | QOR-MATRIX does not. It needs none, and a maker's choice stays theirs. | None for us |
| Redistributing the editor, Engine Tools or a shared engine | Engine licence | Unchanged: not in the image, this repository or the Store's own content (QOR-0020). | None |
| Using a purchased asset in a game | That item's licence | Per item, from the inventory. | Each item's acquisition date and licence |
| Giving an asset to a model, previews included | Content License sections 5 and 17; that item's licence | NoAI content and anything uncertain stay out of every model's input, whether or not it trains. | Those sections' current wording |
| Reselling development assets | That item's licence | Not from the owner's collection, ever: ownership is not resale permission (QOR-0021). | None |
| UEFN, Fortnite and Verse | Their own terms | Out of scope. | None |

**Conditions before an agent reads project code, logs or engine source, or automates an editor:**

1. The owner has accepted a version of the EULA holding update 22, and that version is recorded here.
2. Someone has read sections 6(e) and 8(a) of that version, or given the assistant their text, and they do not restrict the
   operation.
3. The assistant's provider's handling of input is confirmed from the owner's own account setting or contract. It is not
   inferred.
4. For anything touching assets, the inventory exists: listing, publisher, source, acquisition record, asset version, permitted
   uses, NoAI status and evidence, built from the owner's own list or export. An item whose permission is uncertain stays
   unresolved, kept away from redistribution and agents, and work continues with what is cleared.

## Consequences

- **Nothing changes yet,** and QQ and Qt work needs none of this. No engine is on this machine, and none is downloaded before the
  owner has accepted the licence.
- **What the owner is asked for** is narrow: which EULA version they accepted, the text of the two sections and of the Content
  License's sections 5 and 17, their assistant provider's data setting, and a Fab export. These are asked for only when a named
  task needs them.
- **Questions for Epic remain** on plugins, stores, and QOR's chain and CGT (QOR-0020's decision 7).
- **A sign this is wrong:** the clause reads otherwise than reported. Then this record changes before it is accepted.
