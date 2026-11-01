# QOR-0022: What assistants may do with Unreal Engine, after Epic's change

- **Status:** Proposed. It takes effect only when the owner accepts it. Until then, `AGENTS.md` §3 and
  [QOR-0020](QOR-0020-unreal-engine-on-qor-os.md)'s decision 8 stand unchanged, and assistants keep to them.
- **Date:** 2026-11-01
- **Deciders:** the owner. Drafted by the assistant, as the owner's brief of 1 November 2026 asked.
- **Would supersede:** QOR-0020's decision 8, and the sentence on assistants in `AGENTS.md` §3
- **Relates to:** [QOR-0021](QOR-0021-projects-engines-and-two-stores.md)

## Context

**QOR-0020 missed this.** It was written on 1 November from Epic's licence of May 2023 and captures from before the change,
two days after Epic changed the clause on generative AI. Its rule against reading the engine rests on a clause Epic has since
altered, by an amount nobody here has yet read.

**What is known, and how:**

| Fact | Source | How it was read |
|------|--------|-----------------|
| On 27 October Tim Sweeney said Epic restricted AI training on outputs, "but we are working to remove this restriction" | [Game Developer](https://www.gamedeveloper.com/programming/epic-is-updating-the-eula-to-allow-ai-training-on-outputs-from-unreal-engine-and-metahuman) | Read in full |
| Epic's post of 29 October deletes two restrictions from section 6(e), for all versions, and gives Epic no rights in a developer's work; existing users accept on Epic's acceptance page | [Epic's post](https://www.unrealengine.com/ue-devs-can-now-choose-to-train-ai-models-on-their-projects) | A search listing's excerpt only |
| The first restriction barred the engine, source code included, as a training input | Epic's post; [The Click](https://www.theclick.gg/unreal-engine-ai-training-eula/) | Excerpt; a paraphrase |
| **Sources disagree on the second:** The Click says MetaHuman materials; a search summary of Epic's post says prompt-based input | The same two | Unresolved, and the deciding point |
| UEFN projects, Fortnite islands and Verse are not covered; third parties' content keeps its own terms; marketplace permissions are not overridden | The Click | A paraphrase |
| The Epic Content License bars NoAI content "as inputs to Generative AI Programs", as well as from datasets and development | [Its change log](https://www.unrealengine.com/eula-change-log/content) | A search listing's excerpt only |
| A NoAI tag means an asset "must not be used for generative AI data collection"; legacy listings say their licence in Details | [Fab's documentation](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab?lang=en-US) | Read |
| Content acquired earlier keeps the terms of its day; Megascans under the Unreal plan are UE-Only Content | [A licensing wiki, 8 October 2026](https://wiki.licenseware.io/wiki/fab-and-epic-content-licensing/) | Read; secondary |

**Not read by anyone here:** the [EULA](https://www.unrealengine.com/eula/unreal)'s current sections 6(e) and 8(a), its
[change log](https://www.unrealengine.com/eula-change-log/unreal), or the
[Content License](https://www.unrealengine.com/eula/content)'s sections 5 and 17. Those pages answered 403 to automated
reading on 1 November, and the assistant did not try to get around that.

**Which agreement the owner has accepted is unknown.** This PC has no Epic launcher record and no engine folder. The owner's
Epic account is the place to find it. Terms change, and a new download may need the amended licence.

## Decision

Proposed: replace the blanket rule with one for each activity, since each has different terms.

| Activity | Rule |
|----------|------|
| **Training a model** on the engine, projects or assets | QOR-MATRIX does not. It needs none, and Epic's opening the door is no reason to. A maker's own choice stays theirs. |
| **An assistant reading project code, build logs and Epic's public documents** | Allowed once the conditions below hold. |
| **An assistant reading the engine's own files** | Allowed only where a task needs it, on the same conditions. The engine is still never in this repository. |
| **Giving asset content to a model**, previews and thumbnails included | Per item. NoAI content and anything uncertain stay out of every model's input, whether or not that model trains. |
| **Editor automation** that handles assets by name without showing them to a model | Separate from the last row, and allowed where the item's licence allows the use. Having permission does not prove an agent has editor control: each is reported separately. |
| **Shipping** the editor, Engine Tools or a shared engine | Unchanged: not in the image, this repository or the Store's own content (QOR-0020). |
| **UEFN, Fortnite and Verse** | Out of scope, under their own terms. |

**Conditions, all of them, before the second and third rows apply:**

1. The owner has accepted a version of the EULA that holds this change, and its version is recorded here.
2. A person has read sections 6(e) and 8(a) of that version, or given the assistant their text, and they do not restrict
   prompt-based input.
3. The assistant's provider does not train on what it is given, under the owner's actual plan and settings. The owner
   confirms that.
4. An inventory of the owner's assets exists before any of them is used: listing, publisher, source, licence, NoAI status,
   engine restrictions, formats, versions and dependencies. An item whose permission is uncertain is kept apart, naming the
   clause or the evidence that is missing, and work continues with what is cleared.

## Consequences

- **Nothing changes yet.** No engine is on this machine, and none is downloaded before the owner has accepted the licence.
- **The owner's part is small:** accept or decline the new EULA, say which version, check the assistant provider's data
  setting, and give the assistant the text of those pages, for example saved as PDFs.
- **Questions for Epic remain,** on plugins, stores, and QOR's chain and CGT (QOR-0020's decision 7).
- **A sign this is wrong:** the clause reads otherwise than reported. Then this record changes before it is accepted.
