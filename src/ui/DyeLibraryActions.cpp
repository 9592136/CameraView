#include "DyeLibraryActions.h"

#include "../imaging/DyeLibrary.h"

DyeLibraryActionResult DyeLibraryActions::Save(
    std::vector<DyeProfile>& dyes,
    const DyeProfileInput& input)
{
    const DyeProfileInputResult parsed = DyeProfileFormParser::Parse(input);
    if (!parsed.IsValid()) {
        return {
            DyeLibraryActionStatus::InvalidInput,
            false,
            std::nullopt,
            std::nullopt,
            parsed.message
        };
    }

    const DyeProfile dye = *parsed.dye;
    const DyeLibraryUpdateResult update = DyeLibrary::UpsertByName(dyes, dye);

    return {
        DyeLibraryActionStatus::Saved,
        true,
        update.index,
        dye,
        L"Dye saved: " + dye.name + L"."
    };
}

DyeLibraryActionResult DyeLibraryActions::DeleteSelected(
    std::vector<DyeProfile>& dyes,
    int selection)
{
    const std::optional<std::size_t> index =
        DyeLibrary::IndexAtSelection(selection, dyes.size());
    if (!index) {
        return {
            DyeLibraryActionStatus::NoSelection,
            false,
            std::nullopt,
            std::nullopt,
            L"Select a dye first."
        };
    }

    const DyeLibraryDeleteResult deletion = DyeLibrary::DeleteAt(dyes, *index);
    if (!deletion.deleted) {
        return {
            DyeLibraryActionStatus::NoSelection,
            false,
            std::nullopt,
            std::nullopt,
            L"Select a dye first."
        };
    }

    return {
        DyeLibraryActionStatus::Deleted,
        true,
        deletion.next_index,
        deletion.removed,
        L"Dye deleted: " + deletion.removed.name + L"."
    };
}
