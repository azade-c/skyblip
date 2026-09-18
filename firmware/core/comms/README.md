# comms

The companion link, from the side that does not know what Bluetooth is. A platform fills `ports::Link`; everything here speaks endpoints, sessions and JSON.

## Three endpoints, two shapes of send

`events::Endpoint` splits the link in three because the three have different owners. NMEA is the traffic picture and it is broadcast: every subscribed central gets the same bytes, and `ports::Link::send()` reports Ok when one of them took it, so a phone whose controller buffers are full cannot end the pass for the tablet beside it. Config and Log are conversations, and they go out through `send_to()`, addressed to the session whose request they answer.

That is why `payload_bytes()` with no argument is the *smallest* payload any connected central negotiated. One NMEA frame is formatted once and goes to all of them, so it has to fit the narrowest. An iPhone at ATT_MTU 185 sitting beside an Android at 247 pulls every broadcast frame down to 182 bytes, and that is correct, not a compromise: the alternative is formatting the picture once per central.

## Sessions

`LinkSessions` is the table, and it is the same object on both platforms: Zephyr's `connected`/`disconnected`/`att_mtu_updated` callbacks drive it on silicon, `raise_link()`/`drop_link()` drive it in the host suite. It exists so the lifecycle rules are proved once by `make test` rather than living inside a Bluetooth callback nothing can reach.

`kMaxSessions` must never exceed `CONFIG_BT_MAX_CONN`. The controller is what actually admits a central; a table larger than the controller's is a table with rows that can never fill, and a table smaller than it means a central the radio accepted that the firmware refuses to serve. A refused connect is counted, not silently served: the bus never hears of that session, so no service answers its frames, and `refused()` is how a bench sees it happened.

Events are one ordered queue rather than a queue per type. A connection is a lifecycle, and an Up read before the Down that came first leaves a service pushing at a link that is gone.

## The claim

Broadcast is for everyone. Configuration is not: two apps writing settings to one device is two apps disagreeing about what is stored, and a prompt authorised on one phone answered on another is a confirmation with no presence behind it.

So `LinkClaim` grants config and log to the first session that writes a command, and holds it until that session disconnects or releases it. A command from any other session is refused with a reason naming the state, never accepted and quietly ignored. The claim is what makes a *dropped* link safe to act on: only the holder's disconnect cancels a pending prompt and closes an upload window, and before the claim existed any second EFB walking out of range did both.

What a claim is not is access control. `CONFIG_BT_SMP` is off on this product, deliberately (see `products/skyblip_go/prj.conf`), so there is no identity behind a session id. The claim stops two cooperating apps from stepping on each other. What stops a hostile one is the MCUboot signature on the image and the confirmation gesture on the device itself.
