/** \class actor
    \brief Virtual base class for player and monster classes.

    Contains all the properties and methods common to monsters and players.
*/

/** \var CrawlHashTable actor::props
    \brief Properties hash table.

    actor keys
    public
    private

    player keys
    public
    private

    monster keys
    public
    "wand_known" For wands, we can't simply rely on item knowledge, because
    if the player has already identified the wand type, item_type_known
    returns true regarding player's knowledge of this specific wand.
    Thus, we track wand knowledge using this key.
    If the property exists, it means the player knows the monster has a wand.
    If it's true, the player also knows it's type.

    private
*/
