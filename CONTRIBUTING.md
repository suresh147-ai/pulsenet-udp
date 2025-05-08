# Contributing to PulseNet

Great. You want to help. Here’s how not to screw it up.

## 🚨 Legal Crap (Read This)

By submitting code, patches, documentation, or anything else to this repository, you agree to license your contribution under the same license as the project: AGPLv3.

If you’re allergic to that idea, don’t send a pull request. Nobody wants to fight you in court over your five-line fix to a logging statement.

We reserve the right to require a signed CLA (Contributor License Agreement) for any non-trivial contributions. This is so we can relicense the code in the future if we need to, or offer commercial licenses to people who don’t want to play by AGPLv3’s rules.
If that offends your open source purity, tough.

## 🛠 Code Style

Read the [C++ Onboarding Manual](CPP-MANUAL.md). Seriously, it's a requirement.

## 🧪 Testing
 - All new features must come with tests.
 - If your patch drops coverage, expect it to get rejected or heavily criticized.
 - Run this before pushing:

```bash
go test -coverprofile=cover.out && go tool cover -func=cover.out
```

If you see numbers below 90% and you’re not working on something untestable (which you probably aren’t), fix it.

## 💡 Feature Proposals

Open an issue first. Don’t waste your time coding a feature nobody wants.

## 💬 Communication

Don’t show up with bikeshed arguments or vague requests. Be precise. Be useful. If you’re here to “ask a question” and not contribute, try Stack Overflow.

⸻

Want to move fast? Stick to the rules.

Want to debate philosophy? Wrong repo.